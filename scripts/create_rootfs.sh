#!/usr/bin/env bash
set -euo pipefail

# Build an ext2 disk image populated with the contents of root-fs/.
# Uses mkfs.ext2 -d to populate without requiring root or loop-mount.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$ROOT_DIR/build"
ROOTFS_DIR="$ROOT_DIR/root-fs"
EXT2_DISK="$BUILD_DIR/ext2disk.img"
EXT2_SIZE_MB=32

mkdir -p "$BUILD_DIR"

if [ -f "$EXT2_DISK" ]; then
    rm -f "$EXT2_DISK"
    echo "[INFO] Removed old ext2 disk image"
fi

if [ ! -d "$ROOTFS_DIR" ]; then
    echo "[ERROR] root-fs/ directory not found at: $ROOTFS_DIR" >&2
    exit 1
fi

echo "[INFO] Creating ext2 disk image (${EXT2_SIZE_MB} MB) from root-fs/..."
dd if=/dev/zero of="$EXT2_DISK" bs=1M count="$EXT2_SIZE_MB" status=none
mkfs.ext2 -F -q -d "$ROOTFS_DIR" "$EXT2_DISK"
echo "[INFO] ext2 disk image created with root-fs/ contents: $EXT2_DISK"
