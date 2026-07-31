#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${SCRIPT_DIR}/install-common.sh"

"${SUDO[@]}" zypper --non-interactive install \
  gcc-c++ \
  make \
  cmake \
  libqt6-qtbase-devel \
  nlohmann_json-devel \
  libopenssl-devel \
  sqlite3-devel \
  wl-clipboard

build_and_install_all
echo "Installed Remote Clipboard binaries to ${PREFIX}/bin"
