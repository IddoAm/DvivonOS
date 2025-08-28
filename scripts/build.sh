#!/bin/bash
set -e

# Always run from project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

# Ensure cross-compiler is in PATH
if [[ ":$PATH:" != *":$HOME/opt/cross/bin:"* ]]; then
	export PATH="$HOME/opt/cross/bin:$PATH"
fi

BUILD_DIR=build

# If CMake cache references missing source files, wipe build dir.
# This checks for any source file reference within the CMake cache
# that doesn't exist in the current source tree.
if [ -d "$BUILD_DIR" ] && grep -q "\.c" "$BUILD_DIR/CMakeFiles"/* 2>/dev/null; then
	echo "[INFO] Stale source detected, wiping build dir..."
	rm -rf "$BUILD_DIR"
fi

# Create build folder if missing
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake if not configured yet
if [ ! -f Makefile ]; then
	cmake ..
fi

# Build kernel.elf only
make kernel.elf
