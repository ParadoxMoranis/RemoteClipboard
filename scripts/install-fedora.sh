#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${SCRIPT_DIR}/install-common.sh"

"${SUDO[@]}" dnf install -y \
  gcc-c++ \
  make \
  cmake \
  qt6-qtbase-devel \
  nlohmann-json-devel \
  openssl-devel \
  sqlite-devel \
  wl-clipboard

build_and_install_all
echo "Installed Remote Clipboard binaries to ${PREFIX}/bin"
