#ifndef VGA_H
#define VGA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void vga_initialize(void);
void vga_putchar(char c);
void vga_writestring(const char* data);
void vga_clear(void);

#endif