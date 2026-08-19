# Companion Device production baseline

The Companion Device uses the Xiaozhi revision, board variant, chip target, and
toolchain policy in `companion/production-baseline.json`. Keep that manifest
machine-readable. Update it only after the replacement revision passes the
simulator, device build, and physical hardware checks in this document.

The production firmware and macOS simulator compile the same
`main/product_ui/product_ui.c` translation unit. The Waveshare board supplies
only the LVGL display, touch, backlight, and audio adapters.

## Build the macOS simulator

Install CMake, pkg-config, and SDL2. Configure the device once to download the
pinned LVGL dependency, or set `LVGL_SOURCE_DIR` to an LVGL 9.5 source tree.

```sh
"./simulator/run.sh" --smoke-test
"./simulator/run.sh"
```

The smoke test uses SDL's headless video driver unless the caller supplies a
different `SDL_VIDEODRIVER`.

## Build the Device Firmware

Activate ESP-IDF v6.0.2, then use Xiaozhi's canonical variant builder:

```sh
source "/absolute/path/to/esp-idf-v6.0.2/export.sh"
python3 "scripts/build.py" "waveshare/esp32-s3-touch-lcd-1.54" \
  --name "esp32-s3-touch-lcd-1.54"
```

Do not add Wi-Fi credentials, Hermes credentials, tokens, private keys, or
certificates to this repository. Provision runtime secrets through the existing
device flow or an ignored local configuration.

## Hardware baseline checks

Do these checks after a separately approved flash. A successful build does not
replace them.

1. Confirm that `^ TOP ^` is upright and centered at the physical top edge.
2. Confirm the four swatches appear as red, green, blue, and white from left to
   right. This detects orientation, RGB order, and inversion errors.
3. Drag across all four corners. Confirm the displayed touch coordinates follow
   the finger from approximately `0, 0` to `239, 239` without swapped axes.
4. Confirm the restored backlight is stable and that the existing power-button
   double-click brightness behavior still works.
5. Enter Wi-Fi configuration mode. Use the power-button action to start the
   existing AudioService audio test, speak into both microphones, then use the
   action again to stop and play the recording through the speaker. The queue is
   bounded to 10 seconds. Treat missing capture and missing playback as separate
   failures and retain the corresponding serial log.

## Factory recovery boundary

The immutable factory image is outside this repository at
`../backup/flash-full-16mb-20260818.bin`. It is 16,777,216 bytes and its SHA-256
digest is:

```text
368365e59f93eaf18c71e1df8a5d5378cd83e7301ae12ab2cc8653e5dc5a6093
```

Verify it before any approved hardware flash:

```sh
shasum -a 256 "../backup/flash-full-16mb-20260818.bin"
```

Restoring the image overwrites the complete 16 MB flash, including device
state. Run the following command only after explicit approval, with the correct
serial port selected:

```sh
COMPANION_SERIAL_PORT="/dev/cu.usbmodem..."
python -m esptool --chip esp32s3 --port "${COMPANION_SERIAL_PORT}" \
  --baud 460800 write_flash 0x0 "../backup/flash-full-16mb-20260818.bin"
```
