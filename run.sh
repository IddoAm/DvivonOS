#!/usr/bin/env bash
set -euo pipefail

# set env
source scripts/set_env.sh

# Default behavior: build kernel with custom bootloader unless --grub is specified
BUILD_DIR=build
USE_GRUB=0
CLEAN=0
GDB=0
NO_CLOSE=0
RESET_FS=0

# Parse flags
while (( "$#" )); do
  case "$1" in
    --gdb|-g)
      GDB=1
      shift
      ;;
    --no-close|-n)
      NO_CLOSE=1
      shift
      ;;
    --grub)
      USE_GRUB=1
      shift
      ;;
    --clean|-c)
      CLEAN=1
      shift
      ;;
    --reset-fs|-r)
      RESET_FS=1
      shift
      ;;
    -h|--help)
      cat <<EOF
Usage: $(basename "$0") [--grub] [--gdb|-g] [--no-close|-n] [--clean|-c] [--reset-fs|-r]
  --grub        Use GRUB ISO as bootloader (default is custom bootloader)
  --gdb, -g     Start QEMU with GDB stub (-S -s)
  --no-close,-n Prevent QEMU from closing/rebooting (-no-reboot -no-shutdown)
  --clean, -c   Clean the build directory before building
  --reset-fs,-r Rebuild ext2 disk image from root-fs/ directory contents
EOF
      exit 0
      ;;
    *)
      shift
      ;;
  esac
done

# -------------------------------
# Script paths
# -------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/scripts"

# Ensure required helper scripts exist
if [ ! -x "$SCRIPT_DIR/build_grub.sh" ] || [ ! -x "$SCRIPT_DIR/pack.sh" ] || [ ! -x "$SCRIPT_DIR/run_qemu.sh" ] || [ ! -x "$SCRIPT_DIR/build_bootloader.sh" ] || [ ! -x "$SCRIPT_DIR/create_rootfs.sh" ]; then
  echo "[ERROR] Required scripts missing or not executable in: $SCRIPT_DIR" >&2
  exit 1
fi

if [ "$CLEAN" -eq 1 ]; then
  echo "[INFO] Cleaning build directory..."
  "$SCRIPT_DIR/clean.sh"
fi

if [ "$RESET_FS" -eq 1 ]; then
  echo "[INFO] Rebuilding ext2 disk from root-fs/..."
  "$SCRIPT_DIR/create_rootfs.sh"
fi

# Assemble QEMU args to forward
QEMU_ARGS=()
if [ "$GDB" -eq 1 ]; then
  QEMU_ARGS+=( -S -s )
  echo "gdb -x scripts/debug_memory.gdb"
fi
if [ "$NO_CLOSE" -eq 1 ]; then
  QEMU_ARGS+=( -no-reboot -no-shutdown )
fi

if [ "$USE_GRUB" -eq 1 ]; then
  echo "[INFO] Building kernel and creating GRUB ISO..."
  "$SCRIPT_DIR/build_grub.sh"
  "$SCRIPT_DIR/pack.sh"
  echo "[INFO] Running QEMU (GRUB ISO)..."
  "$SCRIPT_DIR/run_qemu.sh" "${QEMU_ARGS[@]}"
else
  echo "[INFO] Building kernel with custom bootloader..."
  # build_bootloader.sh handles building the padded kernel and complete image
  if [ ${#QEMU_ARGS[@]} -gt 0 ]; then
    "$SCRIPT_DIR/build_bootloader.sh" --complete "${QEMU_ARGS[@]}"
  else
    "$SCRIPT_DIR/build_bootloader.sh" --complete
  fi
fi