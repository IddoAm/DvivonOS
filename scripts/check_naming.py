#!/usr/bin/env python3
"""
Naming convention checker for OS project.
Enforces:
- Constants: UPPER_SNAKE_CASE (LIKE_THAT)
- Variables: snake_case (like_that)
- Functions: snake_case (like_that)
- Types: snake_case with _t suffix or CamelCase
"""

import re
import sys
import os
from pathlib import Path
from typing import List, Tuple

# Patterns
UPPER_SNAKE_CASE = re.compile(r'^[A-Z][A-Z0-9_]*$')
SNAKE_CASE = re.compile(r'^[a-z][a-z0-9_]*$')
CAMEL_CASE = re.compile(r'^[A-Z][a-zA-Z0-9]*$')

# C keywords that should be ignored
C_KEYWORDS = {
    'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
    'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if',
    'int', 'long', 'register', 'return', 'short', 'signed', 'sizeof', 'static',
    'struct', 'switch', 'typedef', 'union', 'unsigned', 'void', 'volatile', 'while',
    'inline', 'restrict', '_Bool', '_Complex', '_Imaginary'
}

# Standard types that should be ignored
STANDARD_TYPES = {
    'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
    'int8_t', 'int16_t', 'int32_t', 'int64_t',
    'size_t', 'ssize_t', 'ptrdiff_t', 'uintptr_t', 'intptr_t',
    'FILE', 'NULL', 'true', 'false'
}

errors: List[Tuple[str, int, str]] = []


def is_constant_name(name: str) -> bool:
    """Check if identifier should be a constant (UPPER_SNAKE_CASE)."""
    # Check if it's a #define
    # This is handled separately in check_defines
    return False


def check_defines(content: str, filepath: str) -> None:
    """Check #define constants are UPPER_SNAKE_CASE."""
    lines = content.split('\n')
    for i, line in enumerate(lines, 1):
        # Match #define MACRO_NAME or #define MACRO_NAME value
        match = re.match(r'^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\s', line)
        if match:
            name = match.group(1)
            # Ignore header guards
            if name.endswith('_H') or name.startswith('_'):
                continue
            # Check if it's UPPER_SNAKE_CASE
            if not UPPER_SNAKE_CASE.match(name):
                errors.append((filepath, i, f"#define '{name}' should be UPPER_SNAKE_CASE (LIKE_THAT)"))
        # Also check for #define with parentheses (function-like macros)
        match = re.match(r'^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(', line)
        if match:
            name = match.group(1)
            if not UPPER_SNAKE_CASE.match(name):
                errors.append((filepath, i, f"#define macro '{name}' should be UPPER_SNAKE_CASE (LIKE_THAT)"))


def check_variables_and_functions(content: str, filepath: str) -> None:
    """Check variables and functions are snake_case."""
    lines = content.split('\n')
    
    # Remove comments and strings to avoid false positives
    content_clean = content
    # Remove single-line comments
    content_clean = re.sub(r'//.*', '', content_clean)
    # Remove multi-line comments (simple version)
    content_clean = re.sub(r'/\*.*?\*/', '', content_clean, flags=re.DOTALL)
    # Remove string literals
    content_clean = re.sub(r'"[^"]*"', '""', content_clean)
    content_clean = re.sub(r"'[^']*'", "''", content_clean)
    
    # Pattern for function definitions: type name(...) or type name(...) {
    func_pattern = re.compile(
        r'(?:^|\s)(?:static\s+|inline\s+|extern\s+)?'
        r'(?:const\s+|volatile\s+|unsigned\s+|signed\s+)?'
        r'(?:struct\s+|union\s+|enum\s+)?'
        r'[a-zA-Z_][a-zA-Z0-9_]*\s*\*?\s*'  # return type
        r'([a-zA-Z_][a-zA-Z0-9_]*)\s*\('  # function name
    )
    
    # Pattern for variable declarations: type name;
    var_pattern = re.compile(
        r'(?:^|\s)(?:static\s+|const\s+|volatile\s+|extern\s+|register\s+)?'
        r'(?:const\s+|volatile\s+|unsigned\s+|signed\s+)?'
        r'(?:struct\s+|union\s+|enum\s+)?'
        r'[a-zA-Z_][a-zA-Z0-9_]*\s*\*?\s*'  # type
        r'([a-zA-Z_][a-zA-Z0-9_]*)\s*[;=,\[\)]'  # variable name
    )
    
    # Pattern for typedef: typedef ... name;
    typedef_pattern = re.compile(
        r'typedef\s+.*?\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;'
    )
    
    # Check functions
    for match in func_pattern.finditer(content_clean):
        name = match.group(1)
        if name in C_KEYWORDS or name in STANDARD_TYPES:
            continue
        # Allow main, printf, etc. (standard library functions)
        if name in ['main', 'printf', 'sprintf', 'memset', 'memcpy', 'strlen', 'strcmp']:
            continue
        # Check if it's snake_case
        if not SNAKE_CASE.match(name) and not name.startswith('_'):
            line_num = content[:match.start()].count('\n') + 1
            errors.append((filepath, line_num, f"Function '{name}' should be snake_case (like_that)"))
    
    # Check variables (simplified - may have false positives)
    for match in var_pattern.finditer(content_clean):
        name = match.group(1)
        if name in C_KEYWORDS or name in STANDARD_TYPES:
            continue
        # Skip if it looks like a function call
        if '(' in content_clean[match.end():match.end()+10]:
            continue
        # Check if it's snake_case or starts with underscore (for internal/private)
        if not SNAKE_CASE.match(name) and not name.startswith('_') and not UPPER_SNAKE_CASE.match(name):
            line_num = content[:match.start()].count('\n') + 1
            # Only report if it's clearly not a constant
            if not UPPER_SNAKE_CASE.match(name):
                errors.append((filepath, line_num, f"Variable '{name}' should be snake_case (like_that)"))
    
    # Check typedef names (should end with _t or be CamelCase)
    for match in typedef_pattern.finditer(content_clean):
        name = match.group(1)
        if name in C_KEYWORDS or name in STANDARD_TYPES:
            continue
        line_num = content[:match.start()].count('\n') + 1
        if not (name.endswith('_t') or CAMEL_CASE.match(name)):
            errors.append((filepath, line_num, f"Typedef '{name}' should end with '_t' or be CamelCase"))


def check_file(filepath: Path) -> None:
    """Check a single file for naming conventions."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        check_defines(content, str(filepath))
        check_variables_and_functions(content, str(filepath))
    except Exception as e:
        print(f"Error reading {filepath}: {e}", file=sys.stderr)


def check_file_naming(filepath: Path) -> None:
    """Check if filename is kebab-case."""
    name = filepath.stem  # filename without extension
    # Allow kebab-case, snake_case, or single lowercase words
    if not (re.match(r'^[a-z][a-z0-9_-]*$', name) or name == name.lower()):
        errors.append((str(filepath), 0, f"Filename '{filepath.name}' should be kebab-case (like-this.c) or lowercase"))


def main():
    """Main function."""
    if len(sys.argv) < 2:
        print("Usage: check_naming.py <file1> [file2] ...", file=sys.stderr)
        sys.exit(1)
    
    for filepath_str in sys.argv[1:]:
        filepath = Path(filepath_str)
        if not filepath.exists():
            print(f"Warning: {filepath} does not exist", file=sys.stderr)
            continue
        
        if filepath.suffix in ['.c', '.h']:
            check_file(filepath)
            check_file_naming(filepath)
    
    if errors:
        print("Naming convention violations found:\n", file=sys.stderr)
        for filepath, line, message in errors:
            if line > 0:
                print(f"{filepath}:{line}: {message}", file=sys.stderr)
            else:
                print(f"{filepath}: {message}", file=sys.stderr)
        sys.exit(1)
    else:
        print("All naming conventions OK", file=sys.stderr)
        sys.exit(0)


if __name__ == '__main__':
    main()

