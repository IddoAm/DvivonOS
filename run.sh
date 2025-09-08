#!/usr/bin/env bash
set -euo pipefail

# set env
source scripts/set_env.sh


# Script configuration
BUILD_DIR=build
DEBUG=0

# Parse flags (only -d supported)
for arg in "$@"; do
  case "$arg" in
    -d) DEBUG=1 ;;
    -h|--help)
      cat <<EOF
Usage: $(basename "$0") [-d]    # -d run QEMU in debug mode
EOF
      exit 0
      ;;
    *) ;; # ignore other args
  esac
done

# Determine script directory (project-root/scripts)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/scripts"

# Ensure the helper scripts exist
if [ ! -x "$SCRIPT_DIR/build.sh" ] || [ ! -x "$SCRIPT_DIR/pack.sh" ] || [ ! -x "$SCRIPT_DIR/run_qemu.sh" ]; then
  echo "[ERROR] Required scripts missing or not executable in: $SCRIPT_DIR" >&2
  exit 1
fi

# Run build, pack, and QEMU scripts
"$SCRIPT_DIR/build.sh"
"$SCRIPT_DIR/pack.sh"

if [ "$DEBUG" -eq 1 ]; then
  "$SCRIPT_DIR/run_qemu.sh" -d
else
  "$SCRIPT_DIR/run_qemu.sh"
fi