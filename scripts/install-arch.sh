#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${SCRIPT_DIR}/install-common.sh"

"${SUDO[@]}" pacman -Sy --needed --noconfirm \
  base-devel \
  cmake \
  ninja \
  qt6-base \
  nlohmann-json \
  openssl \
  sqlite \
  wl-clipboard

build_and_install_all
echo "Installed Remote Clipboard binaries to ${PREFIX}/bin"
