#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_ROOT="${OUTPUT_ROOT:-$ROOT_DIR/.dist/debian12}"
IMAGE="${IMAGE:-debian:12}"

if command -v docker >/dev/null 2>&1; then
  CONTAINER_TOOL="docker"
elif command -v podman >/dev/null 2>&1; then
  CONTAINER_TOOL="podman"
else
  echo "docker or podman is required to build Debian 12 compatible binaries" >&2
  exit 1
fi

mkdir -p "${OUTPUT_ROOT}"

"${CONTAINER_TOOL}" run --rm \
  -v "${ROOT_DIR}:/workspace" \
  -w /workspace \
  "${IMAGE}" \
  bash -lc '
    set -euo pipefail
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y \
      build-essential \
      cmake \
      qt6-base-dev \
      qt6-tools-dev-tools \
      nlohmann-json3-dev \
      libssl-dev \
      libsqlite3-dev \
      wl-clipboard

    rm -rf /tmp/rc-build
    cmake -S . -B /tmp/rc-build \
      -DCMAKE_BUILD_TYPE=Release \
      -DRC_BUILD_TESTS=OFF \
      -DRC_BUILD_WINDOWS_GUI=OFF
    cmake --build /tmp/rc-build -j"$(nproc)"

    install -Dm755 /tmp/rc-build/remote-clipboard-server-64mb /workspace/.dist/debian12/remote-clipboard-server-64mb
    install -Dm755 /tmp/rc-build/remote-clipboard-server-512mb /workspace/.dist/debian12/remote-clipboard-server-512mb
    install -Dm755 /tmp/rc-build/remote-clipboard-cli /workspace/.dist/debian12/remote-clipboard-cli
    install -Dm755 /tmp/rc-build/remote-clipboard-gui /workspace/.dist/debian12/remote-clipboard-gui
  '

echo "Debian 12 compatible binaries are in ${OUTPUT_ROOT}"
