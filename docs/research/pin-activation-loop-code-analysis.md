# Code Context: Repeated "enter PIN / insert SIM card" audio loop

## Executive finding

The repeatedly-spoken "PIN code" / "please insert SIM card" prompt is **not** the server
activation challenge. It is the **4G cellular-modem SIM/registration path**:

- `ModemErrorNoSim` fires `PIN_ERROR` + `OGG_ERR_PIN` audio.
- `ModemErrorRegDenied` fires `REG_ERROR` + `OGG_ERR_REG` audio.
- The audio plays because the 4G board reports no-SIM/registration-denied during modem init,
  and the modem init retry loop re-raises the error **multiple times** (ML307: up to 6×,
  10 s apart; NT26 via its `UartEthModem` internal loop), so the same prompt is spoken in a loop.

Only 4G boards (`bread-compact-ml307`, `bread-compact-nt26`, and the dual `minsi-k08-dual`
board whose default network type is ML307/4G) can produce this. A WiFi/default board never
plays the PIN prompt (no SIM involved).

## Files Retrieved (with line ranges)

1. `main/application.cc` (lines 120-169) - network event callback that maps modem errors to
   `Alert(...PIN_ERROR...OGG_ERR_PIN)` and `REG_ERROR...OGG_ERR_REG`.
2. `main/application.cc` (823-833) - `Application::Alert()` calls `audio_service_.PlaySound(sound)`.
3. `main/boards/common/ml307_board.cc` (103-120) - **the concrete retry loop**: up to 6 reg
   retries; each `ErrorInsertPin`/`ErrorRegistrationDenied` fires `ModemErrorNoSim`/`ModemErrorRegDenied`.
4. `main/boards/common/nt26_board.cc` (67-125) - NT26 start/network; maps `UartEthModem` events
   `ErrorNoSim` -> `ModemErrorNoSim`, `ErrorRegistrationDenied` -> `ModemErrorRegDenied`
   (its internal retry loop lives in `UartEthModem`, not in this tree).
5. `main/boards/common/board.h` (29-32) - `NetworkEvent` modem error enum.
6. `scripts/gen_lang.py` (145-160) - generates `OGG_<BASENAME>` for each `.ogg`; so
   `err_pin.ogg` -> `OGG_ERR_PIN`, `err_reg.ogg` -> `OGG_ERR_REG`.
7. `main/assets/locales/*/language.json` (e.g. en-US lines 12-13) - `PIN_ERROR`="Please insert
   SIM card", `REG_ERROR`="Unable to access network...".
8. `main/Kconfig.projbuild` (122-130) - default board per chip; ESP32-S3 default is
   `BOARD_TYPE_BREAD_COMPACT_WIFI` (no modem, no PIN prompt).
9. `main/CMakeLists.txt` (96-112) - board selection mapping `CONFIG_BOARD_TYPE_*` -> board dir.
10. `main/boards/bread-compact-ml307/config.json`, `bread-compact-nt26/config.json` - 4G board build definitions.

## Key code

### 1. The trigger point — `main/application.cc` network callback (lines 149-156)
```cpp
case NetworkEvent::ModemErrorNoSim:
    Alert(Lang::Strings::ERROR, Lang::Strings::PIN_ERROR, "warning",
          Lang::Sounds::OGG_ERR_PIN);
    break;
case NetworkEvent::ModemErrorRegDenied:
    Alert(Lang::Strings::ERROR, Lang::Strings::REG_ERROR, "warning",
          Lang::Sounds::OGG_ERR_REG);
    break;
```
`Alert()` (823-830) plays the sound: `if (!sound.empty()) audio_service_.PlaySound(sound);`

### 2. The retry loop that repeats it — `main/boards.cc` `Ml307Board::NetworkTask` (lines 103-121)
```cpp
int reg_retries = 0;
while (reg_retries < NETWORK_REG_MAX_RETRIES) {            // 6 retries
    auto result = modem_->WaitForNetworkReady();
    if (result == NetworkStatus::Ready) break;
    else if (result == NetworkStatus::ErrorInsertPin)
        OnNetworkEvent(NetworkEvent::ModemErrorNoSim);     // -> PIN prompt spoken
    else if (result == NetworkStatus::ErrorRegistrationDenied)
        OnNetworkEvent(NetworkEvent::ModemErrorRegDenied); // -> REG prompt spoken
    ...
    reg_retries++;
    vTaskDelay(pdMS_TO_TICKS(10000));                      // 10 s between prompts
}
```
So with a modem present but **no SIM** (or registration denied), the PIN/REG prompt is spoken
**up to 6 times, ~10 s apart**. `MODEM_DETECT_MAX_RETRIES = 30` (line 15) and
`NETWORK_REG_MAX_RETRIES = 6` (line 16).

The equivalent for NT26 is `UartEthModem`'s own internal registration loop — not checked into
this tree (`nt26_board.h` line 5 includes `<uart_eth_modem.h>`, which does not exist here), so
its exact retry cadence can't be read locally.

## Architecture / data flow
1. `Application::Initialize()` calls `board.StartNetwork()` exactly once (application.cc line 169).
2. For a 4G board, `StartNetwork()` spawns a task (`Ml3072::NetworkTask`) or drives the modem
   (`UartEthModem`) that polls the module.
3. While registering, if the modem reports NO SIM / registration denied / init-failed, the board
   emits `ModemErrorNoSim` / `ModemErrorRegDenied` / `ModemErrorInitFailed`.
4. `NetworkEvent::*` hits the application callback (application.cc 149-156) -> `Alert(...)` ->
   `PlaySound(OGG_ERR_PIN|OGG_ERR_REG)` -> the .ogg asset is decoded and played by the audio task.
5. Because the modem registration loop re-emits the error on each failed poll, the prompt is
   spoken repeatedly (6× for ML307 at 10 s interval; NT26 via its internal loop). This is the loop.
6. The **activation challenge** (`ShowActivationCode`, application.cc 799-821) is a *separate*
   path that speaks an N-digit code digit-by-digit (digit sounds), NOT `err_pin.ogg` — so the PIN
   prompt is definitely modem-init, not the server challenge.

## Which board?
- No top-level `config.json` in the repo; board selection is per-board `main/boards/<board>/config.json`
  consumed by `scripts/build.py`.
- `main/Kconfig.projbuild` line 128: ESP32-S3 **default** = `BOARD_TYPE_BREAD_COMPACT_WIFI`
  (Wi-Fi only → no PIN prompt). PIN prompt requires a 4G variant:
  - `bread-compact-ml307` (target esp32s3)
  - `bread-compact-nt26` (target esp32s3)
  - `minsi-k08-dual` via `DualNetworkBoard` which persists network type and **defaults to ML307/4G**
    (`dual_network_board.cc` line 28: `settings.GetInt("type", default_net_type)`, defaults to
     ML307(1) when instantiated with default_net_type=1, then `InitializeCurrentBoard()` creates
     `Ml307Board`).
- The user must be building/running one of these 4G boards (or a board carrying the 4G default)
  to hear the PIN prompt. A plain WiFi/default build won't produce it.

## Likely root-cause loop (highest confidence)
`Ml3072Board::NetworkTask` (ml307_board.cc 103-121): with no SIM (or denied registration) it
re-raises `ModemErrorNoSim`/`ModemErrorRegDenied` up to 6 times at 10 s intervals; each event
propagates to application.cc 150-156 and `PlaySound` the `err_pin`/`err_reg` .ogg asset. For
NT26 the equivalent loop is inside `UartEthModem` (source not in this worktree). If the device
re-reboots (e.g. dual-network switch, watchdog, or a firmware that restarts on init failure), the
loop repeats across reboots as well.

## Residual risks / open questions
- `uart_eth_modem.h` / `at_modem.h` (NT26 & ML307 dependencies) are **not present** in this
  worktree (`find` returned nothing). They likely come from an external/private component or a
  registry dependency fetched at build time. Their exact retry cadence for NT26 can't be read here.
- Which board is actually selected depends on the builder's invocation of `scripts/build.py`;
  no top-level `config.json` exists, so I could not confirm the exact variant from repo state.
- Confirmed: the audio does repeat because the modem registration loop re-emits the error event;
  exact number of repeats (NT26) and any reboot-induced repetition were not confirmed from source.

## Start here for a fix
`main/boards.cc/ml307_board.cc` lines 103-121 (the registration retry loop). Options to break the
loop: (a) only raise `ModemErrorNoSim`/`ModemErrorRegDenied` once instead of every retry, or
(b) guard the Alert so the same sound isn't played repeatedly, or (c) rate-limit the modem-error
callbacks in `main/application.cc` around lines 149-162.
