// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <arch/i686/idt.h>
#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_BUFFER_SIZE 256
#define SCANCODE_MAP_SIZE 128

typedef enum {
    KC_NONE = 0,
    KC_ESC, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12,
    KC_GRAVE, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_MINUS, KC_EQUAL,
    KC_BSPC, KC_TAB, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_LBR, KC_RBR, KC_ENTER,
    KC_LCTRL, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SEMI, KC_APOS, KC_BSLASH,
    KC_LSHIFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMMA, KC_DOT, KC_SLASH, KC_RSHIFT,
    KC_KP_STAR, KC_LALT, KC_SPACE, KC_CAPS,
    // extended (0xE0)
    KC_HOME, KC_UP, KC_PGUP, KC_LEFT, KC_RIGHT, KC_END, KC_DOWN, KC_PGDN, KC_INS, KC_DEL,
    KC_RCTRL, KC_RALT,
} keycode_t;

typedef struct {
    keycode_t code;      // semantic keycode (e.g., KC_A, KC_LEFT)
    bool      pressed;   // true on make, false on break
    uint8_t   ascii;     // 0 if non-printable; includes control mappings (Ctrl+A -> 0x01)
    struct {
        uint8_t shift:1;
        uint8_t ctrl:1;
        uint8_t alt:1;
        uint8_t caps:1;
    } mods;              // snapshot after processing this event
} key_event;


// Return false is buffer is empty
bool keyboard_read(key_event* event);
void keyboard_callback(interrupt_frame_t* frame);

#endif
