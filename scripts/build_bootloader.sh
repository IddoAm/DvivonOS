#!/usr/bin/env bash
set -euo pipefail

# set env
source scripts/set_env.sh

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BUILD_DIR="$ROOT_DIR/build"

# Parse command line arguments
BUILD_TARGET="bootloader"
RUN_TARGET="run_bootloader"
BOOT_IMG="$BUILD_DIR/bootloader/"

if [[ "${1:-}" == "--complete" ]]; then
    BUILD_TARGET="complete_bootloader"
    RUN_TARGET="run_complete"
    BOOT_IMG+="complete.img"
    echo "[INFO] Building complete bootloader with kernel..."
else
    BOOT_IMG+="boot.img"
    echo "[INFO] Building bootloader only..."
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target "$BUILD_TARGET" -j

if [ ! -f "$BOOT_IMG" ]; then
  echo "[ERROR] Boot image not found at $BOOT_IMG" >&2
  exit 1
fi

echo "[INFO] Build completed successfully!"
echo "[INFO] Starting QEMU..."

# Run QEMU with the bootloader
exec qemu-system-i386 \
    -drive file="$BOOT_IMG",format=raw,if=floppy \
    # -no-reboot -no-shutdown \
    # -S -s \
    


