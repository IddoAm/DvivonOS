#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <arch/i686/idt.h>

typedef enum {
    SCHED_STATE_OFF = 0,
    SCHED_STATE_READY = 2,
    SCHED_STATE_STARTING = 3,
    SCHED_STATE_RUNNING = 4,
} scheduler_state_t;

typedef struct task {
    interrupt_frame_t* context;
    void (*entry)(void);
    struct task* next; 
    uint8_t state;
} task_t;

void scheduler_init();
void task_init(task_t* t, void (*entry)(void), uint32_t* stack_top);

#endif