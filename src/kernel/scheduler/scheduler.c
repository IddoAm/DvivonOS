#include <kernel/scheduler/scheduler.h>
#include <lib/stdio.h>

void scheduler_tick(uint64_t ticks) {
    printf("Scheduler Tick: %d\n", ticks);
}

void scheduler_init() {
    register_timer_callback(scheduler_tick);
}