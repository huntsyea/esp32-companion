#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPOSITORY_DIR}/build/companion-simulator-macos"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"

if [[ "${1:-}" == "--smoke-test" || "${1:-}" == "--render-reference" ||
      "${1:-}" == "--render-state" || "${1:-}" == "--trace-events" ]]; then
    export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
fi

exec "${BUILD_DIR}/companion-simulator" "$@"
