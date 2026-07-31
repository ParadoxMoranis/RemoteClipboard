#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-$ROOT_DIR/.build-linux}"
PREFIX="${PREFIX:-/usr/local}"

if [[ "${EUID}" -ne 0 ]]; then
  SUDO=(sudo)
else
  SUDO=()
fi

install_binary() {
  local source_path="$1"
  local target_name="$2"
  "${SUDO[@]}" install -Dm755 "$source_path" "${PREFIX}/bin/${target_name}"
}

build_and_install_all() {
  mkdir -p "${BUILD_ROOT}"
  cmake -S "${ROOT_DIR}" -B "${BUILD_ROOT}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DRC_BUILD_TESTS=OFF \
    -DRC_BUILD_WINDOWS_GUI=OFF
  cmake --build "${BUILD_ROOT}" -j"$(nproc)"

  install_binary "${BUILD_ROOT}/remote-clipboard-server-64mb" "remote-clipboard-server-64mb"
  install_binary "${BUILD_ROOT}/remote-clipboard-server-512mb" "remote-clipboard-server-512mb"
  install_binary "${BUILD_ROOT}/remote-clipboard-cli" "remote-clipboard-cli"
  install_binary "${BUILD_ROOT}/remote-clipboard-gui" "remote-clipboard-gui"
}
