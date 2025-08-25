#!/bin/bash
set -e

# Always run from project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

BUILD_DIR=build
DEBUG=0

# Check for -d flag
for arg in "$@"; do
    if [ "$arg" == "-d" ]; then
        DEBUG=1
    fi
done

if [ "$DEBUG" -eq 1 ]; then
    echo "Running QEMU in debug mode..."
    qemu-system-i386 -cdrom $BUILD_DIR/myos.iso -serial stdio -m 512 -no-reboot -s -S
else
    echo "Running QEMU normally..."
    qemu-system-i386 -cdrom $BUILD_DIR/myos.iso
fi
