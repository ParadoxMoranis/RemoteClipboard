#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/.build-server}"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/.dist/server}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
RUN_TESTS="${RUN_TESTS:-1}"
JOBS="${JOBS:-$(nproc)}"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DRC_BUILD_SERVERS=ON \
  -DRC_BUILD_CLI=OFF \
  -DRC_BUILD_WAYLAND_GUI=OFF \
  -DRC_BUILD_WINDOWS_GUI=OFF \
  -DRC_BUILD_TESTS="$RUN_TESTS" \
  -DRC_STATIC_GNU_RUNTIMES=OFF
cmake --build "$BUILD_DIR" --parallel "$JOBS"

if [[ "$RUN_TESTS" == "1" ]]; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

cmake --install "$BUILD_DIR" --prefix "$OUTPUT_DIR" --component Server
echo "Server binaries staged in $OUTPUT_DIR/bin"
