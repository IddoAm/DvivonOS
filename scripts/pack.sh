#!/usr/bin/env bash
set -euo pipefail

# Pack kernel into a GRUB-bootable ISO.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$ROOT_DIR/build"

source "$SCRIPT_DIR/set_env.sh"

cmake --build "$BUILD_DIR" --target iso -j

ISO="$BUILD_DIR/myos.iso"
if [ ! -f "$ISO" ]; then
  echo "[ERROR] ISO not found at $ISO" >&2
  exit 1
fi

echo "[INFO] ISO created: $ISO"
