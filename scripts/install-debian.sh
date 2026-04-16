#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "${SCRIPT_DIR}/install-common.sh"

"${SUDO[@]}" apt-get update
"${SUDO[@]}" apt-get install -y \
  build-essential \
  cmake \
  qt6-base-dev \
  qt6-tools-dev-tools \
  nlohmann-json3-dev \
  libssl-dev \
  wl-clipboard

build_and_install_all
echo "Installed Remote Clipboard binaries to ${PREFIX}/bin"
