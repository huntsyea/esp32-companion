# Companion Device validation

Use this runbook to collect release evidence for the Waveshare ESP32-S3 Touch
LCD 1.54 Companion Device. The validation tool is non-destructive. It does not
flash a device, change Hermes, open a listener, or provision a secret.

## Automated evidence

Activate the documented ESP-IDF environment. Then run all safe gates from the
repository root:

```sh
source "/absolute/path/to/esp-idf/export.sh"
python3 "scripts/companion_validation.py" --all-safe \
  --report "build/companion-validation-report.md"
```

The command verifies the clean Git revision and the immutable factory backup.
It runs the complete host suite, the strict headless simulator smoke test, and
the canonical board build. It then checks the firmware target, board selection,
flash layout, size, and SHA-256 digest. Subprocess output is suppressed by
default so that a report cannot capture credentials, audio, or conversation
content. Run a failing underlying command directly when you need its output.

The report uses these states:

- `PASS`: the gate ran and passed, or immutable input matched exactly.
- `FAIL`: the gate failed or the evidence did not match the baseline.
- `NOT RUN`: an optional automated gate was not requested.
- `PENDING`: physical evidence has not been recorded.

Running without `--all-safe` inspects the repository, backup, and existing
firmware artifacts. It leaves the three executable gates as `NOT RUN`.

Run one gate at a time with `--run-host-tests`, `--run-simulator`, or
`--run-firmware-build` when you need to isolate a failure.

## Factory image boundary

The factory backup must remain at
`../backup/flash-full-16mb-20260818.bin`. Before an approved flash, verify both
its 16,777,216-byte size and this exact digest:

```text
368365e59f93eaf18c71e1df8a5d5378cd83e7301ae12ab2cc8653e5dc5a6093
```

```sh
wc -c "../backup/flash-full-16mb-20260818.bin"
shasum -a 256 "../backup/flash-full-16mb-20260818.bin"
```

Do not flash or restore until the operator confirms the exact device and serial
port and gives explicit approval. A full restore overwrites all 16 MB,
including device state.

The firmware flash is also a separate destructive operation. After the operator
gives explicit approval for the selected device and the automated report is
green, use the board flash helper from the repository root:

```sh
COMPANION_FLASH_PORT="/dev/cu.usbmodem..." ./flash.sh
# or: ./flash.sh --build --port /dev/cu.usbmodem...
```

The helper replaces bare `idf.py flash`, which fails silently on this board:
the ESP32-S3 native USB-JTAG/serial has no auto-reset lines. Hold the BOOT
button, tap RESET, and keep holding BOOT until esptool reports it is writing;
`flash.sh` prints this warning and requires an explicit board-name confirmation.

Do not treat that approval as approval for a later factory restore. Confirm the
restore separately with this exact warning:

```text
⚠️ Dangerous Operation Detected
Operation Type: Full ESP32-S3 flash restore
Impact Scope: The selected device's complete 16 MB flash, including device state
Risk Assessment: A wrong serial port or interrupted write can overwrite the wrong device or leave this device unbootable

Please confirm to continue? [requires explicit "yes", "confirm", "continue"]
```

After that separate approval, use an explicit serial port to restore:

```sh
COMPANION_RESTORE_PORT="/dev/cu.usbmodem..."
python -m esptool --chip esp32s3 --port "${COMPANION_RESTORE_PORT}" \
  --baud 460800 write_flash 0x0 "../backup/flash-full-16mb-20260818.bin"
```

Read the complete restored flash to a new temporary file and verify that it is
byte-for-byte identical to the factory image. Keep the temporary file until the
validation is accepted; do not overwrite the factory backup.

```sh
COMPANION_VERIFY_IMAGE="$(mktemp -t companion-restored-flash).bin"
python -m esptool --chip esp32s3 --port "${COMPANION_RESTORE_PORT}" \
  --baud 460800 read_flash 0x0 0x1000000 "${COMPANION_VERIFY_IMAGE}"
shasum -a 256 "${COMPANION_VERIFY_IMAGE}"
cmp "../backup/flash-full-16mb-20260818.bin" "${COMPANION_VERIFY_IMAGE}"
```

The read-back digest must equal the baseline digest above and `cmp` must exit
with status 0.

## Physical acceptance checklist

All items start as `PENDING`. Record the firmware digest, time, operator, and a
concise observation for each result. Do not put credentials, private keys, raw
audio, transcript text, token values, or private certificate material in the
report or serial log.

| Check | Status | Evidence to record |
| --- | --- | --- |
| Display color and orientation | PENDING | Upright `^ TOP ^`; red, green, blue, and white swatches in order |
| Avatar identity and animation | PENDING | `alain00` appearance and idle, listening, thinking, speaking, and error states |
| Touch and navigation | PENDING | All corners track from about `0,0` to `239,239`; avatar and clock gestures work |
| Touch response | PENDING | Measured touch-to-visible-response time during idle and animation |
| Buttons | PENDING | Original actions work; push-to-talk and cancel preserve the remembered screen |
| Microphone | PENDING | Speech is captured without retaining raw audio |
| Speaker | PENDING | TTS is audible and continuous |
| Wi-Fi | PENDING | Device joins the intended LAN after restart |
| Secure Hermes channel | PENDING | WSS, mDNS discovery, and certificate trust succeed without logging secrets |
| Clock | PENDING | Local time and time zone synchronize after network connection |
| Live voice exchange | PENDING | Push-to-talk reaches Hermes, speaking animates with TTS, and the prior screen returns |
| Cancellation | PENDING | Cancel during recording and playback; no stale state or audio follows |
| Recovery | PENDING | Reconnect after Mac mini service restart and device restart without manual repair |
| Runtime resources | PENDING | Free internal heap, free PSRAM, and minimum stack margin for each task |
| Responsiveness | PENDING | Display and animation timing and touch response |
| Audio quality | PENDING | Audio underrun count and continuity |
| Interaction latency | PENDING | End-to-end time from release to first audible response |
| Repeated exchanges | PENDING | Start/end memory across a fixed repeated run; no unbounded loss |

Install or configure the Hermes adapter, listener, certificate, and token only
through a separate approved operation on the Mac mini. The validation tool does
not perform those changes.

## Connect verification (device ↔ Hermes WSS)

Confirm the device reaches the Hermes adapter over the secure WebSocket link
before recording live voice evidence. The adapter is configured on the Mac mini
in a **separate, approved operation**; this runbook only *verifies* the link. Do
not log tokens, private keys, or certificate material.

### Prerequisites

- Hermes is running with the `xiaozhi_companion` platform adapter configured for
  `transport=wss` (default listener `port=8765`), a TLS certificate, and a device
  token store. Device access is gated by the `XIAOZHI_COMPANION_ALLOWED_DEVICES`
  / `XIAOZHI_COMPANION_ALLOW_ALL_DEVICES` policies.
- The device has joined the LAN and is provisioned with a companion connection
  config. In physical mode (`CompanionConnectionMode::kPhysical`) the firmware
  requires WSS and a CA trust anchor, resolves the endpoint via mDNS
  (`wss://<mdns_host>:<port>/xiaozhi/v1`) when the host is advertised, and falls
  back to `fallback_url` otherwise. Loopback development mode accepts plaintext
  `ws://127.0.0.1`.

### 1. Confirm the listener is bound (Mac mini)

```sh
lsof -nP -iTCP:8765 -sTCP:LISTEN
```

Expect an entry bound to the configured `bind_host:port` with TLS. A missing
listener means the adapter is not up; do not proceed.

### 2. Confirm the device can resolve the endpoint

Confirm the device is provisioned with either a reachable `mdns_host` (probe
returns the host) or a `fallback_url` that matches the listener above. The
firmware rejects unsafe hostnames/paths and a missing CA trust anchor in
physical mode, so a misconfigured endpoint surfaces as a `Reject(...)` before
any connection attempt.

### 3. Capture the device boot/connect log

Use the proven boot-log tool and watch for a successful WSS handshake and
session negotiation (do not log credentials):

```sh
python3 tools/boot_capture.py /dev/cu.usbmodem3101 12
```

Success signals: the device resolves the endpoint, establishes TLS, and
negotiates a session on `/xiaozhi/v1`. The serial log must not contain raw
token or certificate material.

### 4. Confirm a registered session on the adapter

In the Hermes adapter logs, confirm the device appears as an active session
(negotiated) on the expected `chat_id`/device id. A session that does not
negotiate, or a device rejected by the allow-list, is a failure.

### Development (no device)

To validate the same protocol path without hardware, drive the adapter's
loopback transport (in `hermes-esp32`) with the desktop protocol harness and its
automated tests. That does not replace the physical WSS link check above.

## Scope limits

This release is push-to-talk and half duplex. Wake word, full duplex, OTA, and
other unlisted behavior are out of scope. Keep every limitation visible in the
final validation report. A successful simulator or firmware build is not proof
of physical hardware acceptance.
