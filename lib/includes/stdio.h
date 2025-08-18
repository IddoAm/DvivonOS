
#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

typedef struct stdio_interface {
	void (*init)(void);
	void (*clear)(void);
	void (*putc)(char c);
	void (*puts)(const char *str);
} stdio_interface_t;

// Set the active stdio interface
void stdio_set_interface(stdio_interface_t *interface);

// Get the active stdio interface
stdio_interface_t *stdio_get_interface(void);

// Modular stdio functions (use the active interface)
void stdio_init(void);
void stdio_clear(void);
void putc(char c);
void puts(const char *str);
int printf(const char *format, ...);

#endif // STDIO_H
