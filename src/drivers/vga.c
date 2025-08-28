#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/os.h>
#include <lib/string.h>
#include <drivers/vga.h>


#define VGA_MEMORY  0xB8000 

size_t vga_row;
size_t vga_column;
uint8_t vga_color;
vga_EOF_func vga_EOF_handler;
uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;


static inline uint8_t _vga_entry_color(enum vga_color fg, enum vga_color bg) 
{
	return fg | bg << 4;
}

static inline uint16_t _vga_entry(unsigned char uc, uint8_t color) 
{
	return (uint16_t) uc | (uint16_t) color << 8;
}


void vga_initialize(void) 
{
	vga_row = 0;
	vga_column = 0;
	vga_color = _vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	vga_EOF_handler = vga_clear;

	vga_clear();
}

void vga_putchar_at(char c, uint8_t color, size_t x, size_t y) 
{
	const size_t index = y * VGA_WIDTH + x;
	vga_buffer[index] = _vga_entry(c, color);
}

void vga_writestring(const char* data)
{
	size_t datalen = strlen(data);
	for (size_t i = 0; i < datalen; i++) {
		vga_putchar(data[i]);
	}
}

void vga_putchar(char c) 
{
	// Line Break
	if(c == '\n'){
		vga_row++;
		vga_column = 0;
	}
	else if (c == '\b')
	{
		if(vga_column > 0){
			vga_column--;
			vga_putchar_at(' ', vga_color, vga_column, vga_row);
		}
	}
	else{
		vga_putchar_at(c, vga_color, vga_column, vga_row);
		if (++vga_column == VGA_WIDTH) {
			vga_column = 0;
			vga_row++;
		}
	}
	if (vga_row == VGA_HEIGHT) {
		vga_EOF_handler();
	}

	vga_move_cursor(vga_row, vga_column);
}

void vga_clear(void)
{
	uint8_t clear_color = _vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			vga_buffer[index] = _vga_entry(' ', clear_color);
		}
	}
	vga_row = 0;
	vga_column = 0;
	vga_move_cursor(vga_row, vga_column);
}


void vga_move_cursor(size_t row, size_t col) {
    uint16_t pos = row * VGA_WIDTH + col;

    outb(0x3D4, 14);              // Tell VGA we’re setting high byte of cursor
    outb(0x3D5, (pos >> 8) & 0xFF);

    outb(0x3D4, 15);              // Low byte
    outb(0x3D5, pos & 0xFF);
}

size_t vga_get_row(void) {
    return vga_row;
}

size_t vga_get_col(void) {
    return vga_column;
}

uint8_t vga_get_color(void) {
    return vga_color;
}

uint16_t* vga_get_buffer(void) {
	return vga_buffer;
}

void vga_set_row(size_t row) {
	if (row >= VGA_HEIGHT) {
		vga_row = VGA_HEIGHT - 1;
	} else {
		vga_row = row;
	}
	vga_move_cursor(vga_row, vga_column);
}

void vga_set_col(size_t col)
{
	if (col >= VGA_WIDTH) {
		vga_column = VGA_WIDTH - 1;
	} else {
		vga_column = col;
	}
	vga_move_cursor(vga_row, vga_column);
}

void vga_set_color(uint8_t color) 
{
	vga_color = color;
}

void vga_set_eof_handler(vga_EOF_func handler) {
    vga_EOF_handler = handler;
}
