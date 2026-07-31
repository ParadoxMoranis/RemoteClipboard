#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -eq 0 ]]; then
  echo "makepkg must run as a regular user, not root" >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/.dist/arch}"
VERSION="$(sed -n 's/^project(RemoteClipboard VERSION \([^ ]*\).*/\1/p' "$ROOT_DIR/CMakeLists.txt")"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

SOURCE_ARCHIVE="$WORK_DIR/RemoteClipboard-$VERSION.tar.gz"
if git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git -C "$ROOT_DIR" archive --format=tar.gz \
    --prefix="RemoteClipboard-$VERSION/" -o "$SOURCE_ARCHIVE" HEAD
else
  tar -C "$ROOT_DIR" -czf "$SOURCE_ARCHIVE" \
    --exclude=.git --exclude='.build*' --exclude=.dist \
    --transform="s,^\./,RemoteClipboard-$VERSION/," .
fi

cp "$ROOT_DIR/packaging/arch/PKGBUILD" "$WORK_DIR/PKGBUILD"
(
  cd "$WORK_DIR"
  makepkg --cleanbuild --force --noconfirm
)

mkdir -p "$OUTPUT_DIR"
cp "$WORK_DIR"/*.pkg.tar.zst "$OUTPUT_DIR/"
echo "Arch Linux client package staged in $OUTPUT_DIR"
