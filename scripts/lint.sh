#!/bin/bash
# Lint script for OS project
# Runs all code quality checks: formatting, naming conventions, and static analysis

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

ERRORS=0

echo "=== Running Code Quality Checks ==="
echo ""

# Check if clang-format is available
if command -v clang-format &> /dev/null; then
    echo "[1/4] Checking code formatting with clang-format..."
    
    # Find all C and header files
    FILES=$(find src -name "*.c" -o -name "*.h" | sort)
    
    FORMAT_ERRORS=0
    for file in $FILES; do
        if ! clang-format --dry-run --Werror "$file" &> /dev/null; then
            echo "  ❌ $file needs formatting"
            FORMAT_ERRORS=$((FORMAT_ERRORS + 1))
        fi
    done
    
    if [ $FORMAT_ERRORS -eq 0 ]; then
        echo "  ✅ All files are properly formatted"
    else
        echo "  ⚠️  $FORMAT_ERRORS file(s) need formatting. Run: clang-format -i <file>"
        ERRORS=$((ERRORS + FORMAT_ERRORS))
    fi
else
    echo "[1/4] ⚠️  clang-format not found, skipping format check"
    echo "      Install with: sudo apt-get install clang-format"
fi

echo ""

# Check if clang-tidy is available
if command -v clang-tidy &> /dev/null; then
    echo "[2/4] Running static analysis with clang-tidy..."
    
    # Create compile_commands.json if it doesn't exist
    if [ ! -f "compile_commands.json" ]; then
        echo "  ℹ️  Generating compile_commands.json..."
        # Try to generate it from CMake
        if [ -d "build" ]; then
            cd build
            cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 2>/dev/null || true
            if [ -f "compile_commands.json" ]; then
                cp compile_commands.json "$PROJECT_ROOT/" 2>/dev/null || true
            fi
            cd "$PROJECT_ROOT"
        fi
    fi
    
    if [ -f "compile_commands.json" ]; then
        TIDY_ERRORS=0
        FILES=$(find src -name "*.c" | head -5)  # Limit to avoid too much output
        for file in $FILES; do
            if clang-tidy "$file" --config-file="$PROJECT_ROOT/.clang-tidy" 2>&1 | grep -q "error:"; then
                TIDY_ERRORS=$((TIDY_ERRORS + 1))
            fi
        done
        
        if [ $TIDY_ERRORS -eq 0 ]; then
            echo "  ✅ Static analysis passed"
        else
            echo "  ⚠️  Found static analysis issues (run clang-tidy on individual files for details)"
            ERRORS=$((ERRORS + TIDY_ERRORS))
        fi
    else
        echo "  ⚠️  compile_commands.json not found, skipping clang-tidy"
        echo "      Generate it with: cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    fi
else
    echo "[2/4] ⚠️  clang-tidy not found, skipping static analysis"
    echo "      Install with: sudo apt-get install clang-tidy"
fi

echo ""

# Check naming conventions
echo "[3/4] Checking naming conventions..."
if command -v python3 &> /dev/null; then
    NAMING_ERRORS=0
    FILES=$(find src -name "*.c" -o -name "*.h" | sort)
    
    for file in $FILES; do
        if ! python3 "$SCRIPT_DIR/check_naming.py" "$file" 2>&1 | grep -q "All naming conventions OK"; then
            python3 "$SCRIPT_DIR/check_naming.py" "$file" 2>&1 | grep -v "All naming conventions OK" || true
            NAMING_ERRORS=$((NAMING_ERRORS + 1))
        fi
    done
    
    if [ $NAMING_ERRORS -eq 0 ]; then
        echo "  ✅ All naming conventions OK"
    else
        echo "  ⚠️  Found $NAMING_ERRORS file(s) with naming convention violations"
        ERRORS=$((ERRORS + NAMING_ERRORS))
    fi
else
    echo "  ⚠️  python3 not found, skipping naming convention check"
fi

echo ""

# Check file naming (kebab-case)
echo "[4/4] Checking file naming (kebab-case)..."
FILE_NAMING_ERRORS=0
FILES=$(find src -name "*.c" -o -name "*.h" -o -name "*.s" | sort)

for file in $FILES; do
    filename=$(basename "$file")
    # Check if filename is kebab-case, snake_case, or simple lowercase
    if ! echo "$filename" | grep -qE '^[a-z][a-z0-9_-]*\.[ch]s?$'; then
        echo "  ❌ $file: filename should be kebab-case (like-this.c) or lowercase"
        FILE_NAMING_ERRORS=$((FILE_NAMING_ERRORS + 1))
    fi
done

if [ $FILE_NAMING_ERRORS -eq 0 ]; then
    echo "  ✅ All filenames follow kebab-case convention"
else
    ERRORS=$((ERRORS + FILE_NAMING_ERRORS))
fi

echo ""
echo "=== Summary ==="
if [ $ERRORS -eq 0 ]; then
    echo "✅ All checks passed!"
    exit 0
else
    echo "❌ Found $ERRORS issue(s) that need to be fixed"
    echo ""
    echo "Quick fixes:"
    echo "  - Format code: find src -name '*.c' -o -name '*.h' | xargs clang-format -i"
    echo "  - Check naming: python3 scripts/check_naming.py <file>"
    exit 1
fi

