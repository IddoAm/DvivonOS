#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>

//init the vga
void init_terminal(void);

// Print a string to the screen
void puts(const char *str);

// Print a character to the screen
void putchar(char c);

// Print formatted output to the screen
int printf(const char *format, ...);

#endif // STDIO_H
