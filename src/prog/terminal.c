#include <prog/terminal.h>
#include <lib/stdio.h>
#include <drivers/vga.h>

bool _is_printable_ascii(uint8_t ascii) {
    return ascii >= ' ' && ascii <= '~';
}

void _handle_non_printable_ascii(uint8_t ascii) {
    // Handle non-printable ASCII characters (e.g., control characters)
    switch (ascii) {
        case 0x0A: // LF
            putc('\n');
            break;
        case 0x08: // BS
            putc('\b');
            break;
        default:
            putc('?');
            break;
    }
}

void _scroll_down_once(void)
{
    // move all the lines one up
    uint16_t* vga_buffer = vga_get_buffer();
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t from_index = y * VGA_WIDTH + x;
            const size_t to_index = (y - 1) * VGA_WIDTH + x;
            vga_buffer[to_index] = vga_buffer[from_index];
        }
    }
    // remove the last line
    uint8_t current_color = vga_get_color();
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga_putchar_at(' ', current_color, x, VGA_HEIGHT - 1);
    }
    // move cursor to the beginning of the last line
    vga_set_col(0);
    vga_set_row(vga_get_row() - 1);
	vga_move_cursor(vga_get_row(), vga_get_col());
}

void terminal_initialize(void) {
    stdio_init();
    vga_set_eof_handler(_scroll_down_once);
	printf("%oWelcome to Iddo and Hillel amazing os!!!!\n", STD_COLOR_MAGENTA);
}

void terminal_handle_keypress(key_event event) {
    if (event.pressed && event.ascii) {
        if (_is_printable_ascii(event.ascii)) {
            putc(event.ascii);
        }else
        {
            _handle_non_printable_ascii(event.ascii);
        }
    }
}
