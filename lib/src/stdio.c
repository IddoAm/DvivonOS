#include <stdio.h>
#include <vga.h>

void init_terminal(void) {
    terminal_initialize();
}

void puts(const char *str) {
    while (*str) {
        putchar(*str++);
    }
}

void putchar(char c) {
    terminal_putchar(c);
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = 0;
    for (const char *p = format; *p; ++p) {
        if (*p == '%') {
            ++p;
            // print char
            if (*p == 'c') {
                char val = (char)va_arg(args, int);
                putchar(val);
                ++count;
            // print string
            } else if (*p == 's') {
                const char *val = va_arg(args, const char *);
                puts(val);
                while (*val++) ++count;
            // print int
            } else if (*p == 'd') {
                int val = va_arg(args, int);
                char buffer[20]; // Buffer for integer to string conversion
                int len = 0;
                if (val < 0) {
                    putchar('-');
                    val = -val;
                    ++count;
                }
                do {
                    buffer[len++] = (val % 10) + '0';
                    val /= 10;
                } while (val > 0);
                for (int i = len - 1; i >= 0; --i) {
                    putchar(buffer[i]);
                    ++count;
                }
            // print hex
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
                    putchar(buffer[i]);
                    ++count;
                }
            } else if (*p == '%') {
                putchar('%');
                ++count; 
            } else {
                putchar('%');
                putchar(*p);
                count += 2;
            }
        } else {
            putchar(*p);
            ++count;
        }
    }
    va_end(args);
    return count;
}
