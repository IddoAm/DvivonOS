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



# Create build folder if missing
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake if not configured yet
if [ ! -f Makefile ]; then
	cmake ..
fi

# Build kernel.elf only
make kernel.elf
