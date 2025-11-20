#ifndef TERMINAL_H
#define TERMINAL_H

#include <drivers/keyboard.h>
#include <stdint.h>

void terminal_initialize(void);
void terminal_handle_keypress(key_event event);
void terminal_close(void);

#endif // TERMINAL_H