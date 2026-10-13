# Companion development environment

Working setup for rapid iteration on the Blobatar companion (Waveshare
ESP32-S3-Touch-LCD-1.54) against the Hermes runtime on the Mac mini. Everything
below was verified working on 2026-08-20.

## Layout

- `esp32-companion/` — the single canonical checkout, branch
  `feat/blobatar-voice-companion` (the former `worktree/blobatar-voice-companion`
  work is merged in; that worktree and branch are gone). Use disposable git
  worktrees for parallel experiments; merge back promptly.
- `../hermes-esp32/` — the Hermes platform adapter (`xiaozhi_companion`),
  deployed to the mini at `~/.hermes/plugins/hermes-esp32`.
- Wire contract between the two: `PROTOCOL.md` in each repo,
  `PROTOCOL_VERSION = 1`, bump in lockstep.

## Toolchain and build

ESP-IDF v6.0.2 (the preferred baseline in `companion/production-baseline.json`)
is installed at `~/esp/esp-idf-v6.0.2` (v5.5.2 remains at `~/esp/esp-idf`).

```sh
source ~/esp/esp-idf-v6.0.2/export.sh
python3 scripts/build.py waveshare/esp32-s3-touch-lcd-1.54
```

`scripts/build.py` reads the board `config.json` and applies its
`sdkconfig_append` (board type, `CONFIG_LANGUAGE_EN_US`, jarvis wake word,
Montserrat fonts). Artifacts land in `build/xiaozhi.bin` and
`build/merged-binary.bin`. If you switch IDF versions, run `idf.py fullclean`
first — a stale CMake cache from another IDF tree fails the bootloader
subproject.

## Flash

```sh
./flash.sh
```

This board's native USB-JTAG has no auto-reset lines: hold BOOT the entire
time esptool is connecting. `flash.sh` walks you through it and verifies the
flash; bare `idf.py flash` fails silently. Details and the connect-verify
runbook step: `docs/companion-validation.md`.

## Seeing the screen

**Inner loop (every save, no hardware)** — the SDL simulator links the real
`main/product_ui/*.c` sources, so it is pixel-parity with the device:

```sh
./simulator/run.sh                       # interactive 240x240 window
./simulator/run.sh --render-state speaking 1200 600 /tmp/frame.ppm
./simulator/run.sh --render-clock <epoch> /tmp/clock.ppm
./simulator/run.sh --render-status /tmp/status.ppm
./simulator/run.sh --trace-navigation right,left,left
```

Render/trace modes run headless (`SDL_VIDEODRIVER=dummy`) in under a second.
Pinned PPM references live in `simulator/reference/`; the suite pixel-diffs
against them:

```sh
python3 -m unittest scripts.tests.test_product_ui_state \
    scripts.tests.test_screen_manager scripts.tests.test_blobatar_reference \
    scripts.tests.test_companion_clock scripts.tests.test_companion_validation \
    scripts.tests.test_companion_baseline
```

**Ground truth (real device)** — the firmware ships an MCP tool
`self.screen.snapshot` (`main/mcp_server.cc`, gated on `CONFIG_LV_USE_SNAPSHOT=y`)
that JPEG-encodes the live LVGL screen and POSTs it multipart to a caller-supplied
URL. The production adapter's wire contract does not yet carry MCP, so
triggering it over the normal device↔Hermes session is a queued backlog item
(see `backlog.md`). Panel readback over SPI is impossible on this board —
`DISPLAY_MISO_PIN` is not wired.

## Adapter deploy (Mac mini)

```sh
cd ../hermes-esp32
./scripts/deploy-to-mini.sh          # tests -> push -> remote tests -> gateway restart -> verify
```

Ships committed HEAD to the installed plugin checkout over SSH, runs the suite
with the runtime's venv Python, restarts `ai.hermes.gateway` via launchd, and
waits for `✓ xiaozhi_companion connected` in the gateway log. One-time setup it
depends on is recorded in the script header. `--no-restart` defers the reload.

The mini is reachable as `mac-mini`, an SSH host alias in `~/.ssh/config`.

## Device authorization (one-time per device)

The board connects to `wss://<mini>:8765/xiaozhi/v1` but stays unauthorized
until its pairing code is approved on the mini:

```sh
ssh mac-mini '~/.hermes/hermes-agent/venv/bin/python -m hermes_cli.main pairing list'
ssh mac-mini '~/.hermes/hermes-agent/venv/bin/python -m hermes_cli.main pairing approve ...'
```

Codes expire, so approve while the device is awake and retrying. Rejected
attempts show in `~/.hermes/logs/gateway.log` as
`Unauthorized user: <mac> (<device>) on xiaozhi_companion`.
