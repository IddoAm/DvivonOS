
# Library Structure

The `lib` directory contains all OS library code and headers, organized as follows:

- `lib/includes/` — Header files (e.g., `stdio.h`, `vga.h`)
- `lib/src/`      — Source files (e.g., `stdio.c`)

## Libraries

- `gdt` — Global Descriptor Table management
- `idt` — Interrupt Descriptor Table management
- `keyboard` — Keyboard input handling
- `stdio` — Standard input/output functions
- `vga` — VGA text mode output functions

## Usage

To use these libraries in your code:

1. Include headers in your source files:
   
	```c
	#include <nameOfLibrary.h>
	```

## Notes

- Output functions use VGA routines (see `src/vga.c`).
- Extend or add new headers and sources in the appropriate folders for more features.
