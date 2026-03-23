#include <arch/i686/idt.h>
#include <arch/i686/io.h>
#include <arch/i686/pic.h>
#include <arch/i686/ports.h>
#include <kernel/time/time.h>
#include <lib/stdio.h>

#define PIT_FREQUENCY 1193180

volatile uint64_t system_ticks = 0;

void timer_interrupt_handler(interrupt_frame_t* frame) {
    system_ticks++;
}

uint64_t get_ticks(){
    return system_ticks;
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

