// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <arch/i686/idt.h>

typedef enum {
    KEY_CHAR,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_ALT,
    KEY_UNKNOWN
} KEY_TYPE;

typedef struct {
    KEY_TYPE type;
    char c; // valid only if type == KEY_CHAR
} key_event;

// Return false is buffer is empty
bool keyboard_read(key_event* event);
void keyboard_callback(interrupt_frame_t* frame);

#endif
