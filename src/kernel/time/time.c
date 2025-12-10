#include <arch/i686/idt.h>
#include <arch/i686/io.h>
#include <arch/i686/pic.h>
#include <arch/i686/ports.h>
#include <kernel/time/time.h>
#include <lib/stdio.h>

#define PIT_FREQUENCY 1193180

volatile uint64_t system_ticks = 0;

#define MAX_TIMER_CALLBACKS 16
static timer_callback_t callbacks[MAX_TIMER_CALLBACKS];
static int callback_count = 0;

void timer_interrupt_handler(interrupt_frame_t* frame) {
    system_ticks++;

    // Call registered callbacks
    for (int i = 0; i < callback_count; i++) {
        if (callbacks[i]) {
            callbacks[i](system_ticks);
        }
    }
}

void clock_init(uint32_t frequency) {
    // Calculate the divisor for the desired frequency
    uint32_t divisor = PIT_FREQUENCY / frequency;

    // Send the command byte to the PIT control port
    outb(PIT_COMMAND_PORT,
         0x36); // Command byte: channel 0, lobyte/hibyte, mode 3 (square wave), binary

    // Send the frequency divisor to the PIT data port (channel 0)
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));        // Send low byte
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF)); // Send high byte

    isr_register_handler(irq_to_vector(0), timer_interrupt_handler);
    pic_clear_mask(0); // Unmask IRQ0 (PIT)
}

int register_timer_callback(timer_callback_t callback) {
    if (callback_count >= MAX_TIMER_CALLBACKS) {
        return 1; // Error: maximum callbacks reached
    }

    callbacks[callback_count++] = callback;
    return 0;
}

void unregister_timer_callback(timer_callback_t callback) {
    for (int i = 0; i < callback_count; i++) {
        if (callbacks[i] == callback) {
            // Shift remaining callbacks down
            for (int j = i; j < callback_count - 1; j++) {
                callbacks[j] = callbacks[j + 1];
            }
            callbacks[--callback_count] = 0;
            return;
        }
    }
}
