#include <arch/i686/idt.h>
#include <arch/i686/io.h>
#include <arch/i686/pic.h>
#include <arch/i686/ports.h>
#include <kernel/time/time.h>
#include <lib/stdio.h>
#include <kernel/heap-allocator.h>

#define PIT_FREQUENCY 1193180

static volatile uint64_t system_ticks = 0;
static uint32_t timer_frequency = 0;
static timer_event_t* event_list = NULL;

void timer_interrupt_handler(interrupt_frame_t* frame) {
    system_ticks++;

    while (event_list && system_ticks >= event_list->target_tick) {
        timer_event_t* event = event_list;

        event_list = event->next;

        if (event->callback) {
            event->callback(event->data);
        }

        kfree((uintptr_t)event);
    }
}

uint64_t get_ticks(){
    return system_ticks;
}

uint64_t ticks_to_ms(uint64_t ticks) {
    return (ticks * 1000) / timer_frequency;
}

uint64_t ms_to_ticks(uint32_t ms) {
    return ((uint64_t)ms * timer_frequency) / 1000;
}

void clock_init(uint32_t frequency) {
    timer_frequency = frequency;
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

timer_event_t* register_timer_event(uint64_t delay, timer_callback_t callback, void* data) {
    timer_event_t* event = (timer_event_t*)kmalloc(sizeof(timer_event_t));
    if (!event) return NULL;

    event->target_tick = get_ticks() + delay;
    event->callback = callback;
    event->data = data;
    event->next = NULL;

    if (!event_list || event->target_tick < event_list->target_tick) {
        event->next = event_list;
        event_list = event;
    } else {
        timer_event_t* curr = event_list;
        while (curr->next && curr->next->target_tick < event->target_tick) {
            curr = curr->next;
        }
        event->next = curr->next;
        curr->next = event;
    }

    return event;
}

void unregister_timer_event(timer_event_t* event) {
    if (!event || !event_list) return;

    if (event_list == event) {
        event_list = event->next;
    } else {
        timer_event_t* curr = event_list;
        while (curr->next && curr->next != event) {
            curr = curr->next;
        }

        if (curr->next == event) {
            curr->next = event->next;
        }
    }

    kfree((uintptr_t)event);
}