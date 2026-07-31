#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
OUTPUT_DIR="${2:-dist}"
PACKAGE_NAME="RemoteClipboard-client-windows-x64"
PACKAGE_ROOT="$PWD/$OUTPUT_DIR/$PACKAGE_NAME"

cmake -E remove_directory "$PACKAGE_ROOT"
cmake -E make_directory "$PACKAGE_ROOT"
cmake --install "$BUILD_DIR" --prefix "$PACKAGE_ROOT" --component Client
windeployqt6 --release --no-translations --compiler-runtime \
  "$PACKAGE_ROOT/bin/remote-clipboard-windows.exe"

while true; do
  copied_runtime=0
  while IFS= read -r -d '' binary; do
    while IFS= read -r dependency; do
      runtime="$MINGW_PREFIX/bin/$dependency"
      if [[ -f "$runtime" && ! -f "$PACKAGE_ROOT/bin/$dependency" ]]; then
        cp "$runtime" "$PACKAGE_ROOT/bin/$dependency"
        copied_runtime=1
      fi
    done < <(objdump -p "$binary" | sed -n 's/.*DLL Name: //p')
  done < <(find "$PACKAGE_ROOT/bin" -type f \
    \( -iname '*.exe' -o -iname '*.dll' \) -print0)

  [[ "$copied_runtime" -eq 0 ]] && break
done

missing_runtime=0
while IFS= read -r -d '' binary; do
  while IFS= read -r dependency; do
    if [[ -f "$MINGW_PREFIX/bin/$dependency" && \
          ! -f "$PACKAGE_ROOT/bin/$dependency" ]]; then
      echo "Missing runtime dependency: $dependency (required by $binary)" >&2
      missing_runtime=1
    fi
  done < <(objdump -p "$binary" | sed -n 's/.*DLL Name: //p')
done < <(find "$PACKAGE_ROOT/bin" -type f \
  \( -iname '*.exe' -o -iname '*.dll' \) -print0)
[[ "$missing_runtime" -eq 0 ]]

cmake -E chdir "$PWD/$OUTPUT_DIR" cmake -E tar cf "$PACKAGE_NAME.zip" \
  --format=zip -- "$PACKAGE_NAME"
echo "Windows client package staged in $PWD/$OUTPUT_DIR/$PACKAGE_NAME.zip"
