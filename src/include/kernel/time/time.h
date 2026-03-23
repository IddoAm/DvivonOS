#ifndef TIME_H
#define TIME_H

#include <stdint.h>

typedef void (*timer_callback_t)(void* data);

typedef struct timer_event {
    uint64_t target_tick;      // Absolute tick when this fires
    timer_callback_t callback; // What to run
    void* data;                // Argument to pass to the callback
    struct timer_event* next;  // Next event in the sorted list
} timer_event_t;

void clock_init(uint32_t frequency);
uint64_t get_ticks();

timer_event_t* register_timer_event(uint64_t delay, timer_callback_t callback, void* data);
void unregister_timer_event(timer_event_t* event);

#endif