
#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

// std color names
typedef enum {
    STD_COLOR_BLACK = 0,
    STD_COLOR_BLUE = 1,
    STD_COLOR_GREEN = 2,
    STD_COLOR_CYAN = 3,
    STD_COLOR_RED = 4,
    STD_COLOR_MAGENTA = 5,
    STD_COLOR_BROWN = 6,
    STD_COLOR_LIGHT_GREY = 7,
    STD_COLOR_DARK_GREY = 8,
    STD_COLOR_LIGHT_BLUE = 9,
    STD_COLOR_LIGHT_GREEN = 10,
    STD_COLOR_LIGHT_CYAN = 11,
    STD_COLOR_LIGHT_RED = 12,
    STD_COLOR_LIGHT_MAGENTA = 13,
    STD_COLOR_LIGHT_BROWN = 14,
    STD_COLOR_WHITE = 15,
} std_color_t;

typedef struct stdio_interface {
    void (*init)(void);
    void (*clear)(void);
    void (*putc)(char c);
    void (*puts)(const char* str);
} stdio_interface_t;

// Set the active stdio interface
void stdio_set_interface(stdio_interface_t* interface);

// Get the active stdio interface
stdio_interface_t* stdio_get_interface(void);

// Modular stdio functions (use the active interface)
void stdio_init(void);
void stdio_clear(void);
void putc(char c);
void puts(const char* str);
int printf(const char* format, ...);

#endif // STDIO_H
