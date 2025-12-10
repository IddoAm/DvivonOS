#ifndef TIME_H
#define TIME_H

#include <stdint.h>

void clock_init(uint32_t frequency);
uint64_t get_ticks();

typedef void (*timer_callback_t)(uint64_t ticks);
int register_timer_callback(timer_callback_t callback);
void unregister_timer_callback(timer_callback_t callback);

#endif