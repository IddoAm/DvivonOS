#include <prog/terminal.h>
#include <lib/stdio.h>
#include <drivers/vga.h>

bool _is_special_key(key_event event) {
    
}

void terminal_initialize(void) {
    stdio_interface_t vga_interface = {
        .init = vga_initialize,
        .clear = vga_clear,
        .putc = vga_putchar,
        .puts = vga_writestring
    };
    stdio_set_interface(&vga_interface);
    stdio_init();
	printf("Welcome to Iddo and Hillel amazing os!!!!\n");
}

void terminal_keypress(key_event event) {
