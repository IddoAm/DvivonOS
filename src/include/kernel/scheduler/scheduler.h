#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <arch/i686/idt.h>
#include <stdint.h>
#include <kernel/vmm.h>
#include <kernel/heap_allocator.h>

#define PROCESS_HEAP_START 0x00601000

#define PROCESS_STACK_TOP 0xBFF00000
#define USER_STACK_PAGES 4

typedef enum {
    SCHED_STATE_OFF = 0,
    SCHED_STATE_READY = 1,
    SCHED_STATE_STARTING = 2,
    SCHED_STATE_RUNNING = 3,
} scheduler_state;

static uint32_t next_tid = 1;
static uint32_t next_pid = 1;

typedef struct task {
    uint32_t tid;
    interrupt_frame_t* context;
    void (*entry)(void);
    struct task* next;
    uint8_t state;
} task_t;

typedef struct process {
    uint32_t pid;   
    heap_context_t* heap;

    // list head is main task
    task_t* task_list_head;
    process_t* next;
} process_t;

void scheduler_init();
void task_init(task_t* t, void (*entry)(void), uint32_t* stack_to);
void process_init(process_t* t, void (*entry)(void));

task_t* get_current_task(void);
process_t* get_current_process(void);

#endif