#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This script builds the Linux CLI and Wayland GUI; use the GitHub Windows artifact on Windows." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/.build-client}"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/.dist/client}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
RUN_TESTS="${RUN_TESTS:-1}"
JOBS="${JOBS:-$(nproc)}"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DRC_BUILD_SERVERS=OFF \
  -DRC_BUILD_CLI=ON \
  -DRC_BUILD_WAYLAND_GUI=ON \
  -DRC_BUILD_WINDOWS_GUI=OFF \
  -DRC_BUILD_TESTS="$RUN_TESTS" \
  -DRC_STATIC_GNU_RUNTIMES=OFF
cmake --build "$BUILD_DIR" --parallel "$JOBS"

if [[ "$RUN_TESTS" == "1" ]]; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

cmake --install "$BUILD_DIR" --prefix "$OUTPUT_DIR" --component Client
echo "Linux client binaries staged in $OUTPUT_DIR/bin"
