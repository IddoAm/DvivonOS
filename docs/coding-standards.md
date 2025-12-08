# Coding Standards

This document outlines the coding standards for the OS project. All code should follow these conventions to maintain consistency and readability.

## Naming Conventions

### Constants
Constants must use **UPPER_SNAKE_CASE** (all uppercase with underscores).

```c
// ✅ Correct
#define MAX_BUFFER_SIZE 1024
#define KERNEL_VIRT 0xC0000000
#define INTURRUPT_COUNT 256

// ❌ Incorrect
#define maxBufferSize 1024
#define kernel_virt 0xC0000000
```

### Variables
Variables must use **snake_case** (lowercase with underscores).

```c
// ✅ Correct
int buffer_size;
uint32_t kernel_address;
static idt_entry idt[256];

// ❌ Incorrect
int bufferSize;
uint32_t kernelAddress;
static idt_entry Idt[256];
```

### Functions
Functions must use **snake_case** (lowercase with underscores).

```c
// ✅ Correct
void idt_init(void);
void isr_register_handler(uint8_t num, interrupt_handler_t handler);
static void internal_function(void);

// ❌ Incorrect
void idtInit(void);
void IsrRegisterHandler(uint8_t num, interrupt_handler_t handler);
```

### Types
Types (typedef, struct, enum) should either:
- End with `_t` suffix: `interrupt_frame_t`, `idt_entry_t`
- Use **CamelCase**: `InterruptFrame`, `IdtEntry`

```c
// ✅ Correct
typedef struct {
    uint32_t value;
} interrupt_frame_t;

typedef struct {
    int count;
} IdtEntry;

// ❌ Incorrect
typedef struct {
    uint32_t value;
} interrupt_frame;  // Missing _t suffix
```

### File Naming
Source files must use **kebab-case** (lowercase with hyphens) or simple lowercase.

```c
// ✅ Correct
heap_allocator.c
heap_allocator.c
heapallocator.c
idt.c
main.c

// ❌ Incorrect
heapAllocator.c
HeapAllocator.c
IDT.c
```

## Code Formatting

We use `clang-format` for consistent code formatting. The configuration is in `scripts/formats/.clang-format`.

### Formatting Rules
- Indentation: 4 spaces (no tabs)
- Column limit: 100 characters
- Braces: Attach style (opening brace on same line)
- Function braces: Always break after function declaration

### Formatting Code
To format a file:
```bash
clang-format -i --style=file:scripts/formats/.clang-format src/path/to/file.c
```

To format all files:
```bash
find src -name "*.c" -o -name "*.h" | xargs clang-format -i --style=file:scripts/formats/.clang-format
```

## Code Quality Checks

### Running All Checks
Run the lint script to check all code quality standards:

```bash
./scripts/formats/lint.sh
```

This will check:
1. Code formatting (clang-format)
2. Static analysis (clang-tidy)
3. Naming conventions (custom script)
4. File naming conventions

### Individual Checks

#### Check Naming Conventions
```bash
python3 scripts/formats/check_naming.py src/path/to/file.c
```

#### Check Formatting
```bash
clang-format --dry-run --Werror --style=file:scripts/formats/.clang-format src/path/to/file.c
```

#### Static Analysis
```bash
clang-tidy src/path/to/file.c --config-file=scripts/formats/.clang-tidy
```

## Pre-commit Checklist

Before committing code, ensure:
- [ ] Code is formatted with `clang-format`
- [ ] All naming conventions are followed
- [ ] No linter errors (`./scripts/lint.sh` passes)
- [ ] File names follow kebab-case convention

## Examples

### Good Example
```c
#include <stdint.h>

#define MAX_HANDLERS 256
#define INTERRUPT_GATE 0x8E

typedef struct {
    uint32_t base;
    uint16_t limit;
} idt_descriptor_t;

static idt_descriptor_t idt_table[MAX_HANDLERS];

void idt_init(void) {
    // Implementation
}

void register_interrupt_handler(uint8_t num, void (*handler)(void)) {
    // Implementation
}
```

### Bad Example
```c
#include <stdint.h>

#define maxHandlers 256  // ❌ Should be MAX_HANDLERS
#define InterruptGate 0x8E  // ❌ Should be INTERRUPT_GATE

typedef struct {
    uint32_t base;
    uint16_t limit;
} IdtDescriptor;  // ❌ Should be idt_descriptor_t

static IdtDescriptor idtTable[MAX_HANDLERS];  // ❌ Should be idt_table

void IdtInit(void) {  // ❌ Should be idt_init
    // Implementation
}

void registerInterruptHandler(uint8_t num, void (*handler)(void)) {  // ❌ Should be register_interrupt_handler
    // Implementation
}
```

## Tools

### Required Tools
- `clang-format` - Code formatting
- `clang-tidy` - Static analysis (optional but recommended)
- `python3` - For naming convention checks

### Installation

On Ubuntu/Debian:
```bash
sudo apt-get install clang-format clang-tidy python3
```

On other systems, install these tools through your package manager.

