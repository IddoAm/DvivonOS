#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include <drivers/keyboard.h>
#include <arch/i686/io.h>
#include <arch/i686/ports.h>

#define KEYBOARD_BUFFER_SIZE 256

// US keyboard scan code set 1 (partial, printable keys)
const char scancode_map[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6',  // 0x00 - 0x07
    '7',  '8',  '9', '0', '-', '=', '\b', '\t', // 0x08 - 0x0F
    'q',  'w',  'e', 'r', 't', 'y', 'u', 'i',  // 0x10 - 0x17
    'o',  'p',  '[', ']', '\n', 0,  'a', 's',  // 0x18 - 0x1F
    'd',  'f',  'g', 'h', 'j', 'k', 'l', ';',  // 0x20 - 0x27
    '\'', '`',  0,  '\\', 'z', 'x', 'c', 'v',   // 0x28 - 0x2F
    'b',  'n',  'm', ',', '.', '/', 0,   '*',   // 0x30 - 0x37
    0,    ' ',  0,    0,   0,   0,   0,   0,    // 0x38 - 0x3F
    0,    0,    0,    0,   0,   0,   0,   0,    // 0x40 - 0x47
    0,    0,    0,    0,   0,   0,   0,   0,    // 0x48 - 0x4F
    0,    0,    0,    0,   0,   0,   0,   0,    // 0x50 - 0x57
    0,    0,    0,    0,   0,   0,   0,   0     // 0x58 - 0x5F
};


key_event keyboard_buffer[KEYBOARD_BUFFER_SIZE] = {0};
volatile uint8_t buffer_head = 0;
volatile uint8_t buffer_tail = 0;

void keyboard_callback() {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    if(scancode < 128 && scancode >= 0){
        key_event event;

        event.type = KEY_CHAR;
        event.c = scancode_map[scancode];

        keyboard_buffer[buffer_head] = event;

        buffer_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    }
  
}

bool keyboard_read(key_event* event){
    if(buffer_head != buffer_tail){
        *event = keyboard_buffer[buffer_tail];
        buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
        return true;
    }
    return false;
}