# touch-ui

An ESP-IDF 5.5 + LVGL 9 starting point for the **Waveshare ESP32-S3-Touch-LCD-1.54**
(ESP32-S3R8, 16 MB flash, 8 MB octal PSRAM, 240 × 240 ST7789 display, CST816 capacitive touch).

It brings up the backlight, display, and touch controller, then draws a small
LVGL screen that reports live touch coordinates and counts button taps — enough
to confirm the whole input-to-pixel path works before you build anything real.

The same UI module also runs in a native macOS simulator. The ESP32 and macOS
entry points only provide platform adapters; both render
`components/app_ui/app_ui.c`.

## Prerequisites

ESP-IDF v5.5.2 installed at `~/esp/esp-idf`:

```bash
git clone -b v5.5.2 --depth 1 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32s3
```

## Build and run

### macOS simulator

The simulator requires CMake, pkg-config, and SDL 2. With Homebrew:

```bash
brew install cmake pkg-config sdl2
```

It reuses the LVGL version pinned by ESP-IDF in `dependencies.lock`. If
`managed_components/` is not present in a fresh checkout, populate it first:

```bash
. ~/esp/esp-idf/export.sh
idf.py reconfigure
```

Run it with:

```bash
./simulator/run.sh
```

The simulator opens a 2x window while preserving the board's logical 240 x 240
resolution. A mouse click behaves like a touch. For a headless build-and-start
check, run:

```bash
SDL_VIDEODRIVER=dummy ./simulator/run.sh --smoke-test
```

### Physical device

```bash
./flash.sh /dev/cu.usbmodem3101
```

Or by hand:

```bash
. ~/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbmodem3101 build flash
idf.py -p /dev/cu.usbmodem3101 erase-otadata   # see "Flash layout" below
idf.py -p /dev/cu.usbmodem3101 monitor
```

## Flash layout

`partitions.csv` deliberately reproduces the layout the board shipped with:

| Partition | Type | Offset | Size | Contents |
| --- | --- | --- | --- | --- |
| `factory` | app | `0x20000` | 6.31 MB | This project |
| `ota_0` | app | `0x670000` | 4 MB | xiaozhi 2.0.5, as shipped |
| `assets` | data / spiffs | `0xa70000` | 4.31 MB | Vendor launcher images and fonts |

Because the board left the factory with otadata pointing at `ota_0`, the
bootloader will keep launching xiaozhi even after you flash this project. Run
`idf.py erase-otadata` once and it falls back to the `factory` slot. Writing a
valid OTA image to `ota_0` later reverses that.

A full 16 MB image of the board as it arrived is in `../backup/`, with a
SHA-256 alongside it. Restore with:

```bash
esptool --port /dev/cu.usbmodem3101 write-flash 0x0 \
    ../backup/flash-full-16mb-20260818.bin
```

## Board notes

Pin assignments live in `main/board.h`, cross-checked between the xiaozhi board
config and Waveshare's Arduino demos.

Things that will bite you if you change them:

- **PSRAM is octal.** `CONFIG_SPIRAM_MODE_OCT` is required or it will not
  initialize. It also consumes GPIO 33–37, which are therefore unavailable.
- **The display needs SPI mode 3 and inverted color.** Any other combination
  produces a negative or scrambled image rather than a blank screen.
- **The CST816 ignores I2C between touches.** Drivers that probe for a chip ID
  at boot can fail; this one reads the ID successfully on this board, but if you
  swap drivers and see a probe failure, that is why.
- **All four strapping pins are in use** (0 and 3 for buttons and charge sense,
  45 and 46 for display DC and backlight). Expect warnings; do not repurpose.
- **Console output goes over the native USB bridge**, not UART0, hence
  `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`. Note that pulsing RTS from a plain
  serial library does not reset this chip — use esptool or the BOOT button.
