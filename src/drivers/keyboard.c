#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <arch/i686/io.h>
#include <arch/i686/ports.h>
#include <drivers/keyboard.h>

#define KBD_DATA 0x60

// ---------- Modifiers & state ----------
static bool s_shift = false;
static bool s_ctrl = false;
static bool s_alt = false;
static bool s_caps = false;
static bool s_e0 = false; // extended prefix active

// ---------- Scancode set 1 → keycode map (make codes) ----------
/* Only common keys; expand as needed */
static const keycode_t sc1_to_keycode[128] = {
    /*00*/ KC_NONE, KC_ESC,     KC_1,    KC_2,     KC_3,      KC_4,
    KC_5,           KC_6,       KC_7,    KC_8,     KC_9,      KC_0,
    KC_MINUS,       KC_EQUAL,   KC_BSPC, KC_TAB,
    /*10*/ KC_Q,    KC_W,       KC_E,    KC_R,     KC_T,      KC_Y,
    KC_U,           KC_I,       KC_O,    KC_P,     KC_LBR,    KC_RBR,
    KC_ENTER,       KC_LCTRL,   KC_A,    KC_S,
    /*20*/ KC_D,    KC_F,       KC_G,    KC_H,     KC_J,      KC_K,
    KC_L,           KC_SEMI,    KC_APOS, KC_GRAVE, KC_LSHIFT, KC_BSLASH,
    KC_Z,           KC_X,       KC_C,    KC_V,
    /*30*/ KC_B,    KC_N,       KC_M,    KC_COMMA, KC_DOT,    KC_SLASH,
    KC_RSHIFT,      KC_KP_STAR, KC_LALT, KC_SPACE, KC_CAPS,   KC_F1,
    KC_F2,          KC_F3,      KC_F4,   KC_F5,
    /*40*/ KC_F6,   KC_F7,      KC_F8,   KC_F9,    KC_F10,    KC_NONE,
    KC_NONE,        KC_NONE,    KC_NONE, KC_NONE,  KC_NONE,   KC_NONE,
    KC_NONE,        KC_NONE,    KC_NONE, KC_NONE,
    /*50*/ KC_NONE, KC_NONE,    KC_NONE, KC_NONE,  KC_NONE,   KC_NONE,
    KC_NONE,        KC_NONE,    KC_NONE, KC_NONE,  KC_NONE,   KC_NONE,
    KC_NONE,        KC_NONE,    KC_NONE, KC_NONE,
    /*60*/ /* rarely used legacy numpad scancodes here ... fill later */
    /*70*/ /* ... */
};

// ---------- Keyboard buffer ----------
#define KEYBOARD_BUFFER_SIZE 256

key_event keyboard_buffer[KEYBOARD_BUFFER_SIZE] = {0};
volatile uint8_t buffer_head = 0;
volatile uint8_t buffer_tail = 0;

// Extended 0xE0 make codes that map differently
static keycode_t _e0_map(uint8_t sc) {
    switch (sc) {
        case 0x1D:
            return KC_RCTRL;
        case 0x38:
            return KC_RALT;
        case 0x47:
            return KC_HOME;
        case 0x48:
            return KC_UP;
        case 0x49:
            return KC_PGUP;
        case 0x4B:
            return KC_LEFT;
        case 0x4D:
            return KC_RIGHT;
        case 0x4F:
            return KC_END;
        case 0x50:
            return KC_DOWN;
        case 0x51:
            return KC_PGDN;
        case 0x52:
            return KC_INS;
        case 0x53:
            return KC_DEL;
        default:
            return KC_NONE;
    }
}

// ---------- ASCII translation ----------
static uint8_t _letter_from_keycode(keycode_t kc, bool shift, bool caps) {
    // letters honor caps XOR shift
    bool upper = (shift ^ caps);
    switch (kc) {
        case KC_A:
            return upper ? 'A' : 'a';
        case KC_B:
            return upper ? 'B' : 'b';
        case KC_C:
            return upper ? 'C' : 'c';
        case KC_D:
            return upper ? 'D' : 'd';
        case KC_E:
            return upper ? 'E' : 'e';
        case KC_F:
            return upper ? 'F' : 'f';
        case KC_G:
            return upper ? 'G' : 'g';
        case KC_H:
            return upper ? 'H' : 'h';
        case KC_I:
            return upper ? 'I' : 'i';
        case KC_J:
            return upper ? 'J' : 'j';
        case KC_K:
            return upper ? 'K' : 'k';
        case KC_L:
            return upper ? 'L' : 'l';
        case KC_M:
            return upper ? 'M' : 'm';
        case KC_N:
            return upper ? 'N' : 'n';
        case KC_O:
            return upper ? 'O' : 'o';
        case KC_P:
            return upper ? 'P' : 'p';
        case KC_Q:
            return upper ? 'Q' : 'q';
        case KC_R:
            return upper ? 'R' : 'r';
        case KC_S:
            return upper ? 'S' : 's';
        case KC_T:
            return upper ? 'T' : 't';
        case KC_U:
            return upper ? 'U' : 'u';
        case KC_V:
            return upper ? 'V' : 'v';
        case KC_W:
            return upper ? 'W' : 'w';
        case KC_X:
            return upper ? 'X' : 'x';
        case KC_Y:
            return upper ? 'Y' : 'y';
        case KC_Z:
            return upper ? 'Z' : 'z';
        default:
            return 0;
    }
}

// number row and symbols with/without shift
static uint8_t _symbol_from_keycode(keycode_t kc, bool shift) {
    switch (kc) {
        case KC_1:
            return shift ? '!' : '1';
        case KC_2:
            return shift ? '@' : '2';
        case KC_3:
            return shift ? '#' : '3';
        case KC_4:
            return shift ? '$' : '4';
        case KC_5:
            return shift ? '%' : '5';
        case KC_6:
            return shift ? '^' : '6';
        case KC_7:
            return shift ? '&' : '7';
        case KC_8:
            return shift ? '*' : '8';
        case KC_9:
            return shift ? '(' : '9';
        case KC_0:
            return shift ? ')' : '0';
        case KC_MINUS:
            return shift ? '_' : '-';
        case KC_EQUAL:
            return shift ? '+' : '=';
        case KC_LBR:
            return shift ? '{' : '[';
        case KC_RBR:
            return shift ? '}' : ']';
        case KC_BSLASH:
            return shift ? '|' : '\\';
        case KC_SEMI:
            return shift ? ':' : ';';
        case KC_APOS:
            return shift ? '"' : '\'';
        case KC_GRAVE:
            return shift ? '~' : '`';
        case KC_COMMA:
            return shift ? '<' : ',';
        case KC_DOT:
            return shift ? '>' : '.';
        case KC_SLASH:
            return shift ? '?' : '/';
        case KC_SPACE:
            return ' ';
        default:
            return 0;
    }
}

static uint8_t _ascii_from_keycode(keycode_t kc, bool shift, bool caps, bool ctrl) {
    // control combinations: Ctrl+A..Z -> 0x01..0x1A
    //! for now we dont need the control key, but if we need in the future we need to change it a
    //! bit cuse it scan codes and not the ascii value
    // if (ctrl)
    // {
    //     if (kc >= KC_A && kc <= KC_Z)
    //     {
    //         return (uint8_t)(kc - KC_A + 1); // A->1, B->2, ...
    //     }
    //     if (kc == KC_LEFT)
    //         return 0; // non-printable; keep as event only
    // }
    // printable
    uint8_t ch = _letter_from_keycode(kc, shift, caps);
    if (ch)
        return ch;
    ch = _symbol_from_keycode(kc, shift);
    if (ch)
        return ch;

    // specials that map to ASCII control chars
    switch (kc) {
        case KC_ENTER:
            return '\n';
        case KC_TAB:
            return '\t';
        case KC_BSPC:
            return '\b';
        case KC_ESC:
            return 27;
        default:
            return 0; // arrows/F-keys produce no ascii
    }
}

// ---------- Translate scancode to event ----------
static keycode_t _decode_make(uint8_t sc) {
    if (s_e0)
        return _e0_map(sc);
    if (sc < 128)
        return sc1_to_keycode[sc];
    return KC_NONE;
}

static keycode_t _decode_break(uint8_t sc) {
    // break code is make|0x80; same keycode map
    sc &= 0x7F;
    return _decode_make(sc);
}

static void _apply_modifier(keycode_t kc, bool pressed) {
    switch (kc) {
        case KC_LSHIFT:
        case KC_RSHIFT:
            s_shift = pressed;
            break;
        case KC_LCTRL:
        case KC_RCTRL:
            s_ctrl = pressed;
            break;
        case KC_LALT:
        case KC_RALT:
            s_alt = pressed;
            break;
        case KC_CAPS:
            if (pressed) {
                s_caps = !s_caps;
            }
            break;
        default:
            break;
    }
}

void keyboard_callback() {
    uint8_t sc = inb(KBD_DATA);

    if (sc == 0xE0) {
        s_e0 = true;
        return;
    }
    // (0xE1 is Pause/Break sequence; omitted for brevity)

    bool break_code = (sc & 0x80) != 0;
    keycode_t kc = break_code ? _decode_break(sc) : _decode_make(sc);

    if (kc != KC_NONE) {
        // Update modifiers and prepare event
        _apply_modifier(kc, !break_code);

        key_event event = {
            .code = kc,
            .pressed = !break_code,
            .ascii = 0,
            .mods = {.shift = s_shift, .ctrl = s_ctrl, .alt = s_alt, .caps = s_caps}};

        if (!break_code) {
            event.ascii = _ascii_from_keycode(kc, s_shift, s_caps, s_ctrl);
        }
        // push to queue
        keyboard_buffer[buffer_head] = event;
        buffer_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    }

    s_e0 = false; // consume extended prefix
}

bool keyboard_read(key_event* event) {
    if (buffer_head != buffer_tail) {
        *event = keyboard_buffer[buffer_tail];
        buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
        return true;
    }
    return false;
}