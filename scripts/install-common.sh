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

build_project() {
  local source_dir="$1"
  local build_name="$2"
  cmake -S "${ROOT_DIR}/${source_dir}" -B "${BUILD_ROOT}/${build_name}" -DCMAKE_BUILD_TYPE=Release
  cmake --build "${BUILD_ROOT}/${build_name}" -j"$(nproc)"
}

install_binary() {
  local source_path="$1"
  local target_name="$2"
  "${SUDO[@]}" install -Dm755 "$source_path" "${PREFIX}/bin/${target_name}"
}

build_and_install_all() {
  mkdir -p "${BUILD_ROOT}"

  build_project "RemoteClipboardServer-64MB" "server64"
  build_project "RemoteClipboardServer-512MB" "server512"
  build_project "RemoteclipboardCliForLinux" "cli"
  build_project "RemoteClipboard_Linux_wayland" "gui"

  install_binary "${BUILD_ROOT}/server64/RemoteClipboardServer" "remote-clipboard-server-64mb"
  install_binary "${BUILD_ROOT}/server512/RemoteClipboardServer" "remote-clipboard-server-512mb"
  install_binary "${BUILD_ROOT}/cli/clipboard_sync" "remote-clipboard-cli"
  install_binary "${BUILD_ROOT}/gui/RemotePostboard" "remote-clipboard-gui"
}
