#!/usr/bin/env bash
set -euo pipefail

source scripts/set_env.sh

# -------------------------------
# Defaults
# -------------------------------
BUILD_DIR=build
USE_GRUB=0
CLEAN=0
GDB=0
NO_CLOSE=0
RESET_FS=0
BUILD_PROGRAMS=0

# -------------------------------
# Parse flags
# -------------------------------
while (( "$#" )); do
  case "$1" in
    --gdb|-g)       GDB=1;      shift ;;
    --no-close|-n)  NO_CLOSE=1; shift ;;
    --grub)         USE_GRUB=1; shift ;;
    --clean|-c)     CLEAN=1;    shift ;;
    --reset-fs|-r)  RESET_FS=1; shift ;;
    --programs|-p)  BUILD_PROGRAMS=1; RESET_FS=1; shift ;;
    -h|--help)
      cat <<EOF
Usage: $(basename "$0") [--grub] [--gdb|-g] [--no-close|-n] [--clean|-c] [--reset-fs|-r] [--programs|-p]
  --grub           Use GRUB ISO as bootloader (default is custom bootloader)
  --gdb, -g        Start QEMU with GDB stub (-S -s)
  --no-close, -n   Prevent QEMU from closing/rebooting (-no-reboot -no-shutdown)
  --clean, -c      Clean the build directory before building
  --reset-fs, -r   Rebuild ext2 disk image from root-fs/ directory contents
  --programs, -p   Build/rebuild programs and update filesystem
EOF
      exit 0
      ;;
    *) shift ;;
  esac
done

# -------------------------------
# Script paths
# -------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/scripts"

for s in build_grub.sh pack.sh run_qemu.sh build_bootloader.sh create_rootfs.sh clean.sh; do
  if [ ! -x "$SCRIPT_DIR/$s" ]; then
    echo "[ERROR] Required script missing or not executable: $SCRIPT_DIR/$s" >&2
    exit 1
  fi
done

# -------------------------------
# 1. Clean
# -------------------------------
if [ "$CLEAN" -eq 1 ]; then
  echo "[INFO] Cleaning build directory..."
  "$SCRIPT_DIR/clean.sh"
fi

# -------------------------------
# 2. Build programs
# -------------------------------
if [ "$BUILD_PROGRAMS" -eq 1 ]; then
  echo "[INFO] Building programs..."
  cmake --build "$BUILD_DIR" --target programs
fi


# -------------------------------
# 3. Filesystem
# -------------------------------
EXT2_DISK="$BUILD_DIR/ext2disk.img"

if [ "$RESET_FS" -eq 1 ]; then
  echo "[INFO] Rebuilding ext2 disk from root-fs/..."
  "$SCRIPT_DIR/create_rootfs.sh"
elif [ ! -f "$EXT2_DISK" ]; then
  echo "[INFO] ext2 disk not found, creating from root-fs/..."
  "$SCRIPT_DIR/create_rootfs.sh"
fi


# -------------------------------
# 4. Build
# -------------------------------
if [ "$USE_GRUB" -eq 1 ]; then
  echo "[INFO] Building kernel (GRUB)..."
  "$SCRIPT_DIR/build_grub.sh"
  echo "[INFO] Packing GRUB ISO..."
  "$SCRIPT_DIR/pack.sh"
else
  echo "[INFO] Building bootloader + kernel..."
  "$SCRIPT_DIR/build_bootloader.sh"
fi

# -------------------------------
# 5. Assemble QEMU arguments
# -------------------------------
QEMU_ARGS=()

if [ "$USE_GRUB" -eq 1 ]; then
  QEMU_ARGS+=( -cdrom "$BUILD_DIR/myos.iso" )
else
  QEMU_ARGS+=( -drive "file=$BUILD_DIR/bootloader/complete.img,format=raw,if=floppy" )
fi

QEMU_ARGS+=( -drive "file=$EXT2_DISK,format=raw,if=ide" )

if [ "$GDB" -eq 1 ]; then
  QEMU_ARGS+=( -S -s )
  echo "[INFO] GDB stub enabled — connect with: gdb -x scripts/debug_memory.gdb"
fi

if [ "$NO_CLOSE" -eq 1 ]; then
  QEMU_ARGS+=( -no-reboot -no-shutdown )
fi

# -------------------------------
# 6. Run
# -------------------------------
if [ "$GDB" -eq 1 ]; then
  echo "[INFO] Starting QEMU in the background..."
  "$SCRIPT_DIR/run_qemu.sh" "${QEMU_ARGS[@]}" &
  QEMU_PID=$!
  
  # Give QEMU a split second to open the port before GDB tries to connect
  sleep 0.5 
  
  echo "[INFO] Launching GDB..."
  gdb -x scripts/debug_memory.gdb
  
  # Optional: Automatically kill QEMU when you exit GDB
  kill $QEMU_PID 
else
  echo "[INFO] Starting QEMU..."
  sleep 1
  "$SCRIPT_DIR/run_qemu.sh" "${QEMU_ARGS[@]}"
fi