#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

mapfile -t SOURCES < <(
  rg --files client_common server_common tests \
    -g '*.cpp' \
    -g '*.h' \
    | sort
)

clang-format --dry-run --Werror "${SOURCES[@]}"
