#!/usr/bin/env bash
set -e

# Always run from project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

BUILD_DIR=build

# Forward all extra arguments to QEMU
QEMU_ARGS="$@"

echo "[INFO] Starting QEMU with args: $QEMU_ARGS"

qemu-system-i386 \
    -cdrom "$BUILD_DIR/myos.iso" \
    -serial stdio \
    -m 512 \
    -no-reboot \
    $QEMU_ARGS
