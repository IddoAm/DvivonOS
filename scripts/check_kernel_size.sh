#!/bin/bash
set -euo pipefail

KERNEL_BIN="$1"
MAX_SIZE="$2"

# Get kernel size
KERNEL_SIZE=$(stat -c%s "$KERNEL_BIN" 2>/dev/null || echo 0)

echo "  Actual size: $KERNEL_SIZE bytes"

# Check if kernel size exceeds maximum
if [ "$KERNEL_SIZE" -gt "$MAX_SIZE" ]; then
    echo "WARNING: Kernel size ($KERNEL_SIZE bytes) exceeds maximum allowed size ($MAX_SIZE bytes)!" >&2
    exit 1
else
    echo "Kernel size is within limits."
fi
