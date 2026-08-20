#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-macos"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"

APP_BUNDLE="${BUILD_DIR}/touch-ui-simulator.app"
APP_EXECUTABLE="${APP_BUNDLE}/Contents/MacOS/touch-ui-simulator"

if [[ "${1:-}" == "--smoke-test" ]]; then
    exec "${APP_EXECUTABLE}" "$@"
fi

exec open -W "${APP_BUNDLE}" --args "$@"
