#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

typedef struct task {
    uint32_t* esp;
    void (*entry)(void);
    struct task* next; 
    uint8_t state;
} task_t;

void scheduler_init();
void task_init(task_t* t, void (*entry)(void), uint32_t* stack_top);
void start_first_task();

#endif