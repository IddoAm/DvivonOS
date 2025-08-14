// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KEY_CHAR,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_ALT,
    KEY_UNKNOWN
} key_type;

typedef struct {
    key_type type;
    char c; // valid only if type == KEY_CHAR
} key_event;

bool keyboard_read(key_event* event);

#endif
