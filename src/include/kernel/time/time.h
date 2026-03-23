#ifndef TIME_H
#define TIME_H

#include <stdint.h>

void clock_init(uint32_t frequency);
uint64_t get_ticks();

#endif