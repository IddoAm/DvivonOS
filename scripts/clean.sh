#!/usr/bin/env bash
set -euo pipefail

# Remove all build artifacts.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/.."

rm -rf "$ROOT_DIR/build" "$ROOT_DIR/iso"

echo "[INFO] Cleaned build/ and iso/ directories"
