#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <arch/i686/idt.h>
#include <stdint.h>
#include <kernel/vmm.h>
#include <kernel/heap-allocator.h>

#define PROCESS_HEAP_START 0x00601000

#define PROCESS_STACK_TOP 0xBFF00000
#define USER_STACK_PAGES 4

typedef enum {
    SCHED_STATE_OFF = 0,
    SCHED_STATE_READY = 1,
    SCHED_STATE_STARTING = 2,
    SCHED_STATE_RUNNING = 3,
} scheduler_state; //TODO: think if it need to end with _t

typedef struct process {
    const uint32_t pid;

    interrupt_frame_t* context; 
    uint32_t pd_phys;
    
    uint32_t kernel_stack_top;
    uint32_t heap_brk;

    struct process* next;
} process_t;

void scheduler_init();
void scheduler_start();
void process_init(process_t* t, void (*entry)(void));
void process_exit(process_t* proc, interrupt_frame_t* frame);

void scheduler_add_process(process_t* proc);
bool process_load_user_memory(process_t* p, uint32_t vaddr, const void* src, size_t len);

process_t* get_current_process(void);

#endif