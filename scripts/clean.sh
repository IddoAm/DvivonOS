#!/bin/bash
set -e

# Always run from project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

# Remove build artifacts and ISO folder
rm -rf build iso

echo "Cleaned build/ and iso/ directories."
