# Research: XiaoZhi ESP32 firmware looping on "PIN code" / "enter PIN" / "请先输入PIN" prompt

## Summary

A XiaoZhi (xiaozhi-esp32) device that repeatedly speaks a "PIN code / enter PIN / 请先输入PIN" prompt is almost always stuck in the **device activation/registration flow**, not a SIM card problem: the device connects to the OTA server, the server issues a 6-digit activation verification code (激活验证码), and the firmware **re-broadcasts the code every retry until the user binds the device** in the `xiaozhi.me` console. The root cause is that the device is **unbound (never registered) or already bound to a different account** — the activation challenge can never succeed, so the prompt repeats indefinitely. On 4G/ML307 boards the "Please insert SIM card" (err_pin.ogg / `PIN_ERROR`) variant has a second, hardware-level cause: the SIM card has **PIN-code verification enabled** (`sim.getStatus()==2`), which the firmware cannot satisfy, so it loops on the PIN error. The official fix is to bind the device in the control panel using the read-out code (or unbind it from the old account via `xiaozhi.ai@tenclass.com`), and, for the SIM-PIN case, disable the SIM's PIN verification.

## Findings

1. **Activation verification-code flow is the dominant cause of the "PIN/enter PIN" loop.** On first boot the device POSTs firmware info to the OTA URL (`https://api.tenclass.net/xiaozhi/ota/`); the server, if the device isn't registered, returns an `activation` block containing `code` (6-digit), `message`, `challenge`, `timeout_ms`. The firmware `ShowActivationCode()` speaks the code through the speaker (and on screen/serial) and then **long-polls `POST …/activate` repeatedly**, re-announcing the code each retry, until the user adds the device in the console. py-xiaozhi documents the retry loop explicitly: max 60 tries, 5 s interval, 5 min total timeout, "每次重试时重新播放验证码提示" (re-play the code each retry). [xiaozhi.esp32-server #745](https://github.com/xinnan-tech/xiaozhi-esp32-server/issues/745) · [py-xiaozhi 设备激活流程](https://huangjunsen0406.github.io/py-xiaozhi/zh/guide/设备激活流程.html) · [Boardor: Registering devices via OTA interface](https://boardor.com/blog/xiao-zhi-ai-communication-protocol-fix-registering-devices-via-ota-interface)

2. **Root cause #1: device never bound / code not consumed.** The device cannot leave activation until the operator enters the 6-digit code in the `xiaozhi.me` console (控制台 → 添加设备). Until then every wake/re-poll re-speaks the code — an endless "enter PIN" loop. Official flow: register phone, click add device, input the 6-digit code shown/spoken by the device. [xiaozhi.dev FAQ Q0/Q1](https://xiaozhi.dev/en/docs/usage/faq/) · [mpython xiaozhi tutorial 激活设备](https://mpython-esp32s3-doc.readthedocs.io/zh-cn/latest/4_xiaozhi/3_xiaozhi.html) · [Issue #1529 "Setup Xiaozhi without display or serial"](https://github.com/78/xiaozhi-esp32/issues/1529)

3. **Root cause #2: Device already bound to another account → the code is spoken but binding fails, looping forever.** Official answer to a user who saw the activation code but couldn't bind: "你这个已经被别人绑定了" (this device is already bound to another account). Fix: email unbinding to `xiaozhi.ai@tenclass.com` with subject "【解绑设备，设备ID XXX，MAC地址 XXX】", then re-add. [Issue #1471 "没有播报验证码"](https://github.com/78/xiaozhi-esp32/issues/1471) · [mpython 解绑教程](https://mpython-esp32s3-doc.readthedocs.io/zh-cn/latest/4_xiaozhi/3_xiaozhi.html)

4. **Root cause #3 (4G/cellular only): SIM PIN verification enabled → `PIN_ERROR` / err_pin.ogg "Please insert SIM card".** On ML307 Cat.1 boards the modem reports `PIN_ERROR` via `AT+CPIN?` when the SIM card has PIN enabled (`sim.getStatus()==2`) or is not inserted/detected (`==0`). QuecPython docs instruct: enter the correct PIN, **disable the SIM PIN**, restart; if the PIN is already wrong 3× the card locks (status 3) and needs the PUK. This is a hardware-level loop distinct from the activation-code flow, and it's what triggers the `err_pin.ogg`/`PIN_ERROR` localized string. [QuecPython SIM Troubleshooting](https://developer.quectel.com/doc/quecpython/Application_guide/zh/network-comm/sim/Troubleshoot-SIM-card-abnormalities.html) · [xiaozhi-esp32 ML307 CSDN troubleshooting: "SIM卡未插入 → PIN_ERROR"](https://aihardware.csdn.net/69cba6a754b52172bc65c04d.html)

5. **Asset & string confirmation.** The assets and locale strings exist in the firmware tree and are the very items the user reported:
   - `main/assets/{zh-CN,en-US,zh-TW,ja-JP}/language.json`: `"PIN_ERROR": "Please insert SIM card"` / `"请插入 SIM 卡"` and `"REG_ERROR": "Unable to access network, please check SIM card status"`.
   - `main/assets/locales/zh-CN/err_pin.ogg` (PIN error), `err_reg.ogg` (registration error), plus `activation.ogg`/`activation` and digit sound 0..9 used by `ShowActivationCode()`.
   These are wired into `main/boards/common/ml307_board.cc` (SIM PIN / REG errors) and `main/application.cc` `ShowActivationCode()` / `CheckNewVersion()` (activation loop). [ai-features doc asset tree](https://xiaozhi.dev/en/docs/ai-features/) · [main/assets language.json diff](https://github.com/78/xiaozhi-esp32) (PIN_ERROR added in the same OTA/activation refactor, e.g. dotnfc/xiaozhi-esp32 commit 3404180).

6. **The activation flow was refactored into a blocking retry loop — a known loop-prone behavior.** `main/application.cc` `CheckNewVersion()` sets `kDeviceStateActivating`, and if the server returns an activation code/challenge it enters a loop: `ShowActivationCode()` then up to 10× `ota_.Activate()` (200 → break; 202/timeout → wait 3 s; other → wait 10 s) until success. If `Activate()` never returns 200 (device not added to console, or already bound elsewhere), the loop repeats the PIN/verification-code announcement. This is the firmware path behind "repeatedly speaks PIN." [main/application.cc diff + main/ota.cc diff](https://github.com/78/xiaozhi-esp32) (v1.6.1 era OTA/activation refactor).

6. **WebSocket/registration caveat (community bug):** If a WebSocket session was already opened before binding, the "add device" action won't take effect — you must rebind/re-establish the WebSocket (and, on self-hosted servers, configure both the OTA URL **and** the WebSocket/MQTT endpoint, else the server speaks "No version information found for this device. Please correctly configure the OTA address." instead of binding). This is a config error, not the PIN loop itself, but it can *look* like the device is stuck. [Board.m](https://boardor.com/blog/xiao-zhi-ai-communication-protocol-fix-registering-devices-via-ota-interface)

## Sources

- Kept:
  - [xiaozhi-esp32 FAQ (xiaozhi.dev)](https://xiaozhi.dev/en/docs/usage/faq/) — official setup/binding and ML307 4G SIM steps.
  - [Issue #1471 没有播报验证码](https://github.com/78/xiaozhi-esp32/issues/1471) — authoritative confirmation that "already bound to another account" blocks activation.
  - [Issue #1529 Setup w/o display](https://github.com/78/xiaozhi-esp32/issues/1529) — maintainer confirms the code is read out over the speaker during activation.
  - [Boardor: Registering via OTA interface](https://boardor.com/blog/xiao-zhi-ai-communication-protocol-fix-registering-devices-via-ota-interface) — exact `activation{code,message,challenge}` JSON, the "enter the verification code: 602245" TTS text, and the bind-then-reconnect caveat.
  - [py-xiaozhi 设备激活流程](https://huangjunsen0406.github.io/py-xiaozhi/zh/guide/设备激活流程.html) — activation retry loop (60×5 s) and replaying the code, and v1/v2 HMAC activation protocol.
  - [QuecPython SIM Troubleshooting](https://developer.quectel.com/doc/quecpython/Application_guide/en/network/sim/Troubleshoot-SIM-card-abnormalities.html) — SIM PIN (status 2) and PUK lock (status 3) → the physical PIN root cause.
  - [CSDN xiaozhi-esp32 排查 guide](https://aihardware.csdn.net/69cba6a754b52172bc65c04d.html) — "SIM卡未插入 → PIN_ERROR" mapping for ML307.
  - [xiaozhi.dev AI-features doc](https://xiaozhi.dev/en/docs/ai-features/) — confirms `err_pin.ogg` / `err_reg.ogg` asset layout.
  - [78/xiaozhi-esp32 repo](https://github.com/78/xiaozhi-esp32) — source for `main/ota.cc`, `main/application.cc` loop, `language.json` strings.
- Dropped:
  - AtomGit/GitCode ML307 articles — third-party SEO rephrasing of the same repo; no new primary facts, low authority.
  - Most AI-features SEO pages — used only for the asset-name confirmation; low signal beyond that.

## Gaps

- I could not verify the exact current retry count / interval on the *latest* xiaozhi-esp32 mainline from a single authoritative source (the 10-retry loop and 60-try/5 min values come from two different versions/implementations). Treat "loops indefinitely while unbound" as the confirmed behavior; the exact numeric cadence may differ by firmware version.
- Whether the device presents as an "enter PIN" vs. a spoken 6-digit number depends on which asset (err_pin vs. activation code digit sounds) fires; on a WiFi board the activation code read-out (0–9.ogg + activation.ogg) is what sounds like a "PIN." Distinguishing the two requires either a serial log (`AT+CPIN?` / `sim.getStatus()`) or knowing whether a SIM module is present.

## Supervisor coordination

No runtime bridge or decision was required. The research targets were reached via direct web search; all key claims are sourced. No further action needed beyond downstream engineering use.

---

## Suggested next-step evidence (for the requesting engineer)

To distinguish the two root causes on a specific device:
1. Watch serial output for `STATE: activating` / repeated `Activating... n/10` → activation-verification-code loop (root cause #1/#2).
2. If a 4G ML307 board is used, query the modem with `AT+CPIN?` / check `sim.getStatus()` → status `2` = SIM PIN enabled (`PIN_ERROR`), status `0` = SIM not detected.
