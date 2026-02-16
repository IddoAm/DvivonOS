#!/usr/bin/env bash
set -euo pipefail

# Build kernel.elf for GRUB boot.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$ROOT_DIR/build"

source "$SCRIPT_DIR/set_env.sh"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target kernel.elf -j

echo "[INFO] kernel.elf built successfully"
