#!/usr/bin/env bash
set -e

# Always run from project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

BUILD_DIR=build

# Forward all extra arguments to QEMU
QEMU_ARGS="$@"

echo "[INFO] Starting QEMU with args: $QEMU_ARGS"

# --- Create ext2 disk image for filesystem testing (once) ---
EXT2_DISK="$BUILD_DIR/ext2disk.img"
if [ ! -f "$EXT2_DISK" ]; then
    echo "[INFO] Creating ext2 disk image (32 MB)..."
    dd if=/dev/zero of="$EXT2_DISK" bs=1M count=32 status=none
    mkfs.ext2 -F -q "$EXT2_DISK"
    echo "[INFO] ext2 disk image created: $EXT2_DISK"
fi

qemu-system-i386 \
    -cdrom "$BUILD_DIR/myos.iso" \
    -drive file="$EXT2_DISK",format=raw,if=ide \
    -serial stdio \
    -m 512 \
    -no-reboot \
    $QEMU_ARGS
