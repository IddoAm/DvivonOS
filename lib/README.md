
# Library Structure

The `lib` directory contains all OS library code and headers, organized as follows:

- `lib/includes/` — Header files (e.g., `stdio.h`, `vga.h`)
- `lib/src/`      — Source files (e.g., `stdio.c`)

## Files

- `includes/stdio.h` — Standard output function declarations
- `includes/vga.h`   — VGA output declarations
- `src/stdio.c`      — Implementation of standard output functions
- `src/vga.c`        — Implementation of VGA output functions

## Usage

To use these libraries in your code:

1. Add the `lib/includes` directory to your compiler's include path:
   
    ```sh
	gcc -Ilib/includes ...
	```

2. Include headers in your source files:
   
	```c
	#include <nameOfLibrary.h>
	```

3. Link with the corresponding source files (e.g., `lib/src/stdio.c`).

## Notes

- Output functions use VGA routines (see `src/vga.c`).
- Extend or add new headers and sources in the appropriate folders for more features.
