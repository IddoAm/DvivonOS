#ifndef VGA_H
#define VGA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// consts
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
typedef void (*vga_EOF_func)(void);

enum vga_color {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

//outer functions
void vga_initialize(void);
void vga_putchar_at(char c, uint8_t color, size_t x, size_t y);
void vga_putchar(char c);
void vga_writestring(const char* data);
void vga_clear(void);
void vga_move_cursor(size_t row, size_t col);

//getters and setters for - cursor position, color, EOF handler
size_t vga_get_row(void);
size_t vga_get_col(void);
uint8_t vga_get_color(void);
// vga_EOF_func vga_get_eof_handler(void);
void vga_set_row(size_t row);
void vga_set_col(size_t col);
void vga_set_color(uint8_t color);
void vga_set_eof_handler(vga_EOF_func handler);

#endif