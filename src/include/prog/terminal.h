#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <drivers/keyboard.h>

void terminal_initialize(void);
void terminal_keypress(key_event event);
void terminal_close(void);

#endif // TERMINAL_H