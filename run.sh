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

# Run build, pack, and QEMU scripts from scripts directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/scripts"

# Build kernel.elf
"$SCRIPT_DIR/build.sh"

# Pack kernel.elf into ISO
"$SCRIPT_DIR/pack.sh"

# Run QEMU (pass -d if needed)
if [ "$DEBUG" -eq 1 ]; then
    "$SCRIPT_DIR/run_qemu.sh" -d
else
    "$SCRIPT_DIR/run_qemu.sh"
fi
