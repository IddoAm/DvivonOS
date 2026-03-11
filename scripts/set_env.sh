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
