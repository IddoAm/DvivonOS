#include <lib/stdio.h>
#include <lib/string.h>
#include <drivers/vga.h>


stdio_interface_t active_interface = {
    .clear = NULL,
    .init = NULL,
    .putc = NULL,
    .puts = NULL
};


void _set_color(std_color_t color) {
    uint8_t vga_color = vga_get_col();
    vga_color = (vga_color & 0xF0) | color; // clear current text color
    vga_set_color(vga_color);
}

void stdio_set_interface(stdio_interface_t *interface) {
    if (interface) {
        if (interface->clear) active_interface.clear = interface->clear;
        if (interface->init) active_interface.init = interface->init;
        if (interface->putc) active_interface.putc = interface->putc;
        if (interface->puts) active_interface.puts = interface->puts;
    }
}

stdio_interface_t *stdio_get_interface(void) {
    return &active_interface;
}

// Modular stdio functions
void stdio_init(void) {
    // if (!active_interface) active_interface = &vga_interface;
    if (active_interface.init) {active_interface.init();}
    else {
        active_interface.clear = vga_clear;
        active_interface.putc = vga_putchar;
        active_interface.puts = vga_writestring;
        active_interface.init = vga_initialize;
        active_interface.init();
    }
}

void stdio_clear(void) {
    if (active_interface.clear) active_interface.clear();
}

void putc(char c) {
    if (active_interface.putc) active_interface.putc(c);
}

void puts(const char *str) {
    if (active_interface.puts) active_interface.puts(str);
}

// supported codes:
// %c - character
// %s - string
// %d - decimal
// %x - hexadecimal
// %% - percent sign
// %o - color sign
int printf(const char *format, ...) {
    uint8_t vga_color = vga_get_color();
    va_list args;
    va_start(args, format);
    int count = 0;
    for (const char* p = format; *p; ++p) {
        if (*p == '%') {
            ++p;
            if (*p == 'c') {
                char val = (char)va_arg(args, int);
                putc(val);
                ++count;
            } else if (*p == 's') {
                const char* val = va_arg(args, const char*);
                puts(val);
                while (*val++)
                    ++count;
            } else if (*p == 'd') {
                int val = va_arg(args, int);
                char buffer[20];
                int len = 0;
                if (val < 0) {
                    putc('-');
                    val = -val;
                    ++count;
                }
                do {
                    buffer[len++] = (val % 10) + '0';
                    val /= 10;
                } while (val > 0);
                for (int i = len - 1; i >= 0; --i) {
                    putc(buffer[i]);
                    ++count;
                }
            } else if (*p == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                char buffer[20];
                int len = 0;
                do {
                    int digit = val % 16;
                    buffer[len++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'a');
                    val /= 16;
                } while (val > 0);
                for (int i = len - 1; i >= 0; --i) {
                    putc(buffer[i]);
                    ++count;
                }
            } else if (*p == 'o') {
                std_color_t color = va_arg(args, std_color_t);
                _set_color(color);
            } else if (*p == '%') {
                putc('%');
                ++count;
            } else {
                putc('%');
                putc(*p);
                count += 2;
            }
        } else {
            putc(*p);
            ++count;
        }
    }
    //restore the color
    vga_set_color(vga_color);
    va_end(args);
    return count;
}
