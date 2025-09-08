#!/usr/bin/env bash
set -euo pipefail

# set env
source scripts/set_env.sh


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target bootloader -j

BOOT_BIN="$BUILD_DIR/bootloader/boot.bin"
if [ ! -f "$BOOT_BIN" ]; then
  echo "[ERROR] boot.bin not found at $BOOT_BIN" >&2
  exit 1
fi

exec qemu-system-i386 -drive file="$BOOT_BIN",format=raw,if=floppy


