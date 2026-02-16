#!/usr/bin/env bash
set -euo pipefail

# Build the complete bootloader + kernel image.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$ROOT_DIR/build"

source "$SCRIPT_DIR/set_env.sh"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target complete_bootloader -j

BOOT_IMG="$BUILD_DIR/bootloader/complete.img"
if [ ! -f "$BOOT_IMG" ]; then
  echo "[ERROR] Boot image not found at $BOOT_IMG" >&2
  exit 1
fi

echo "[INFO] Bootloader build completed: $BOOT_IMG"
