#!/usr/bin/env bash
#
# flash.sh — flash the Companion firmware onto the Waveshare ESP32-S3-Touch-LCD-1.54.
#
# This is the board-specific one-liner for the proven flash path. It replaces the
# bare `idf.py flash`, which fails silently on this board because the ESP32-S3's
# native USB-JTAG/serial has no auto-reset lines: the chip must be put into
# download mode manually BEFORE the tool connects.
#
# Usage:
#   ./flash.sh                     # flash the current build/ on the default port
#   ./flash.sh --build             # run scripts/build.py first, then flash
#   ./flash.sh --port /dev/cu.xxx  # override the serial port
#   ./flash.sh --help
#
# Env:
#   COMPANION_FLASH_PORT   serial port override (same as --port)
#
# Required: the build artifacts must already exist (run `--build` or scripts/build.py).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOARD="waveshare/esp32-s3-touch-lcd-1.54"
BOARD_NAME="esp32-s3-touch-lcd-1.54"

# Intel USB-Serial/JTAG bridge as exposed by this board. Override with --port
# or COMPANION_FLASH_PORT if the board enumerates on a different device.
DEFAULT_PORT="/dev/cu.usbmodem3101"
PORT="${COMPANION_FLASH_PORT:-$DEFAULT_PORT}"
DO_BUILD=0

usage() {
    awk 'NR > 1 { if (/^#/) { sub(/^# ?/, ""); print } else { exit } }' "$0"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)
            DO_BUILD=1
            shift
            ;;
        --port|-p)
            PORT="${2:-}"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "flash.sh: unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "${PORT}" ]]; then
    echo "flash.sh: no serial port given (use --port or COMPANION_FLASH_PORT)" >&2
    exit 2
fi

# ---------------------------------------------------------------------------
# 1. Optional build
# ---------------------------------------------------------------------------
if [[ "$DO_BUILD" -eq 1 ]]; then
    echo ">> Building ${BOARD} (${BOARD_NAME})..."
    if command -v idf.py >/dev/null 2>&1; then
        python3 "${REPO_ROOT}/scripts/build.py" "${BOARD}" --name "${BOARD_NAME}"
    else
        echo "flash.sh: --build requires an active ESP-IDF environment (source export.sh)." >&2
        exit 1
    fi
fi

# ---------------------------------------------------------------------------
# 2. Verify artifacts exist
# ---------------------------------------------------------------------------
for image in \
    "build/bootloader/bootloader.bin" \
    "build/partition_table/partition-table.bin" \
    "build/ota_data_initial.bin" \
    "build/xiaozhi.bin" \
    "build/generated_assets.bin"; do
    if [[ ! -f "${REPO_ROOT}/${image}" ]]; then
        echo "!! flash.sh: missing build artifact ${image} (build first)" >&2
        exit 1
    fi
done

# ---------------------------------------------------------------------------
# 3. Destructive-operation gate
# ---------------------------------------------------------------------------
echo ""
echo "=========================================================="
echo "⚠️  DANGEROUS OPERATION — FLASHES THE DEVICE"
echo "=========================================================="
echo "Target board: ${BOARD} (${BOARD_NAME})"
echo "Serial port : ${PORT}"
echo ""
echo "⚠️  The ESP32-S3 native USB-JTAG has NO auto-reset. You MUST:"
echo "  1. Press and HOLD the BOOT button"
echo "  2. Tap RESET once"
echo "  3. KEEP HOLDING BOOT until flashing starts (~5s in)"
echo "  4. Release BOOT once esptool reports it is downloading/writing"
echo "=========================================================="
echo "This overwrites the selected device's application, bootloader, and"
echo "assets. An interrupted write can leave the device unbootable."
read -r -p "Type the board name to confirm and flash: ${BOARD_NAME} -> " confirmation
if [[ "${confirmation}" != "${BOARD_NAME}" ]]; then
    echo "flash> confirmation mismatch; aborting." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# 4. Resolve esptool and flash
# ---------------------------------------------------------------------------
# Prefer the IDF python env; fall back to `uvx --from esptool` (as boot_capture.py does).
if python3 -c 'import esptool' >/dev/null 2>&1; then
    run_esptool() { python3 -m esptool "$@"; }
elif command -v uvx >/dev/null 2>&1; then
    run_esptool() { uvx --from esptool esptool "$@"; }
else
    echo "!! esptool is not available. Activate ESP-IDF (source export.sh) or install esptool." >&2
    exit 1
fi

cd "${REPO_ROOT}"
run_esptool --chip esp32s3 --port "${PORT}" -b 460800 \
    --before default_reset --after hard_reset \
    write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
    0x0     build/bootloader/bootloader.bin \
    0x8000  build/partition_table/partition-table.bin \
    0xd000  build/ota_data_initial.bin \
    0x20000 build/xiaozhi.bin \
    0x800000 build/generated_assets.bin

echo "flash> done. Device has been hard-reset."
