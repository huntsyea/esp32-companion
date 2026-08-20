#!/usr/bin/env bash
#
# Build, flash, and boot this project on the Waveshare ESP32-S3-Touch-LCD-1.54.
#
# The app is written to the `factory` slot so xiaozhi in `ota_0` and the vendor
# assets partition are left alone. The bootloader keeps preferring ota_0 while
# otadata is valid, so we clear otadata to make it fall back to factory.
set -euo pipefail

PORT="${1:-/dev/cu.usbmodem3101}"

# shellcheck disable=SC1091
. "$HOME/esp/esp-idf/export.sh" > /dev/null

idf.py -p "$PORT" build flash
idf.py -p "$PORT" erase-otadata
idf.py -p "$PORT" monitor
