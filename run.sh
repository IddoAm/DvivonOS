#!/bin/bash
set -e   # stop if any command fails

# check if need to export path
if [[ ":$PATH:" != *":$HOME/opt/cross/bin:"* ]]; then
    export PATH="$HOME/opt/cross/bin:$PATH"
fi
BUILD_DIR=build
DEBUG=0

# Check for -d flag
for arg in "$@"; do
    if [ "$arg" == "-d" ]; then
        DEBUG=1
    fi
done

# Create build folder if missing
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure CMake if not configured yet
if [ ! -f Makefile ]; then
    cmake ..
fi

# Build only changed files
make iso

# Go back to root
cd ..

# Run QEMU
if [ "$DEBUG" -eq 1 ]; then
    echo "Running QEMU in debug mode..."
    qemu-system-i386 -cdrom $BUILD_DIR/myos.iso -serial stdio -m 512 -no-reboot -s -S
    # -s = listen on port 1234 for GDB
    # -S = freeze CPU at startup, wait for GDB
else
    echo "Running QEMU normally..."
    qemu-system-i386 -cdrom $BUILD_DIR/myos.iso
fi
