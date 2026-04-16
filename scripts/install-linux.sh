#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

detect_and_run() {
  if [[ -f /etc/os-release ]]; then
    . /etc/os-release
    case "${ID:-}" in
      ubuntu|debian|linuxmint|pop)
        exec "${SCRIPT_DIR}/install-debian.sh"
        ;;
      arch|manjaro|endeavouros)
        exec "${SCRIPT_DIR}/install-arch.sh"
        ;;
      fedora|rhel|rocky|almalinux)
        exec "${SCRIPT_DIR}/install-fedora.sh"
        ;;
      opensuse*|sles)
        exec "${SCRIPT_DIR}/install-opensuse.sh"
        ;;
    esac
  fi

  echo "Unsupported Linux distribution. Use one of the distro-specific scripts in scripts/."
  exit 1
}

detect_and_run
