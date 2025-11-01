#include <lib/stdio.h>
// #include <vga.h>


static stdio_interface_t *active_interface = NULL;
// // Default interface, can be replaced by user-defined interfaces
// static stdio_interface_t vga_interface = {
//     .init = vga_initialize,
//     .clear = vga_clear,
//     .putc = vga_putchar,
//     .puts = vga_writestring
// };

void stdio_set_interface(stdio_interface_t *interface) {
    active_interface = interface;
}

stdio_interface_t *stdio_get_interface(void) {
    return active_interface;
}

// Modular stdio functions
void stdio_init(void) {
    // if (!active_interface) active_interface = &vga_interface;
    if (active_interface && active_interface->init) active_interface->init();
}

void stdio_clear(void) {
    if (active_interface && active_interface->clear) active_interface->clear();
}

void putc(char c) {
    //asm volatile("cli");
    if (active_interface && active_interface->putc) active_interface->putc(c);
    //asm volatile("sti");
}

void puts(const char *str) {
    if (active_interface && active_interface->puts) active_interface->puts(str);
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = 0;
    for (const char *p = format; *p; ++p) {
        if (*p == '%') {
            ++p;
            if (*p == 'c') {
                char val = (char)va_arg(args, int);
                putc(val);
                ++count;
            } else if (*p == 's') {
                const char *val = va_arg(args, const char *);
                puts(val);
                while (*val++) ++count;
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
    va_end(args);
    return count;
}
