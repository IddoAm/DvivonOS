#!/usr/bin/env bash
set -euo pipefail

# Launch QEMU with common defaults and forwarded arguments.

exec qemu-system-i386 \
    -serial stdio \
    -m 512 \
    "$@"
