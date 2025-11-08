#!/usr/bin/env bash
set -euo pipefail

# -------------------------------
# Autodiscover cross-toolchain
# -------------------------------
find_toolchain() {
    if command -v i686-elf-gcc >/dev/null 2>&1; then
        dirname "$(command -v i686-elf-gcc)"
        return 0
    fi

    for p in /usr/local/cross /opt/cross "$HOME/opt/cross" "$HOME/cross-i686" "$HOME/cross"; do
        if [ -x "$p/bin/i686-elf-gcc" ]; then
            printf '%s\n' "$p/bin"
            return 0
        fi
    done

    for base in /usr /home /opt; do
        found=$(find "$base" -maxdepth 3 -type f -name 'i686-elf-gcc' 2>/dev/null | head -n1 || true)
        if [ -n "$found" ]; then
            dirname "$found"
            return 0
        fi
    done

    return 1
}

TC_BINDIR="$(find_toolchain)" || {
    echo '[ERROR] i686-elf-gcc not found. Install the cross toolchain or add it to PATH.' >&2
    exit 1
}

case ":$PATH:" in
    *":$TC_BINDIR:"*) ;;
    *) export PATH="$TC_BINDIR:$PATH" ;;
esac

# -------------------------------
# Configuration
# -------------------------------
BUILD_DIR=build
DEBUG=0
GDB=0

# -------------------------------
# Parse flags
# -------------------------------
for arg in "$@"; do
    case "$arg" in
        -d) DEBUG=1 ;;
        -g) GDB=1 ;;
        -h|--help)
            cat <<EOF
Usage: $(basename "$0") [-d] [-g]
  -d     Run QEMU in debug mode (normal logging)
  -g     Run QEMU paused with GDB stub enabled and auto-attach
EOF
            exit 0
            ;;
        *) ;; # ignore other args
    esac
done

# -------------------------------
# Script paths
# -------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/scripts"

for s in build.sh pack.sh run_qemu.sh; do
    if [ ! -x "$SCRIPT_DIR/$s" ]; then
        echo "[ERROR] Required script missing or not executable: $SCRIPT_DIR/$s" >&2
        exit 1
    fi
done

# -------------------------------
# Build and pack kernel
# -------------------------------
"$SCRIPT_DIR/build.sh"
"$SCRIPT_DIR/pack.sh"

# -------------------------------
# Run QEMU
# -------------------------------
if [ "$GDB" -eq 1 ]; then
    echo "[INFO] Running QEMU paused with GDB stub (localhost:1234) and auto-attaching GDB..."
    
    # Start QEMU in background paused, GDB stub enabled
    "$SCRIPT_DIR/run_qemu.sh" -S -s &
    QEMU_PID=$!
    
    # Wait a moment for QEMU to start
    sleep 1
    
    # Detect GDB binary
    GDB_CMD=${CROSS_GDB:-gdb-multiarch}
    if ! command -v $GDB_CMD >/dev/null 2>&1; then
        echo "[ERROR] GDB not found. Install gdb-multiarch or i686-elf-gdb." >&2
        kill $QEMU_PID
        exit 1
    fi

    # Launch GDB and attach
    $GDB_CMD build/kernel.elf -ex "target remote :1234" -ex "set pagination off" -ex "layout asm"
    
    # Wait for QEMU to finish
    wait $QEMU_PID

elif [ "$DEBUG" -eq 1 ]; then
    echo "[INFO] Running QEMU in debug mode..."
    "$SCRIPT_DIR/run_qemu.sh" -d
else
    echo "[INFO] Running QEMU normally..."
    "$SCRIPT_DIR/run_qemu.sh"
fi