#!/usr/bin/env bash
set -euo pipefail

# Autodiscover cross-toolchain (i686-elf)
find_toolchain() {
  # 1) If tool already on PATH, use that
  if command -v i686-elf-gcc >/dev/null 2>&1; then
    dirname "$(command -v i686-elf-gcc)"
    return 0
  fi

  # 2) Common install prefixes to try
  for p in /usr/local/cross /opt/cross "$HOME/opt/cross" "$HOME/cross-i686" "$HOME/cross"; do
    if [ -x "$p/bin/i686-elf-gcc" ]; then
      printf '%s\n' "$p/bin"
      return 0
    fi
  done

  # 3) Quick limited search (fast)
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

# Prepend discovered bin dir to PATH if missing
case ":$PATH:" in
  *":$TC_BINDIR:"*) ;;
  *) export PATH="$TC_BINDIR:$PATH" ;;
esac

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