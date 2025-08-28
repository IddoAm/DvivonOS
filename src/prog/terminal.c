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

void terminal_initialize(void) {
    stdio_init();
	printf("Welcome to Iddo and Hillel amazing os!!!!\n");
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
