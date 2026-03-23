#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <arch/i686/idt.h>
#include <stdint.h>
#include <kernel/vmm.h>
#include <kernel/heap-allocator.h>
#include <fs/vfs/file.h>
#include <kernel/time/time.h>

#define PROCESS_HEAP_START 0x00601000

#define PROCESS_STACK_TOP 0xBFF00000
#define USER_STACK_PAGES 4

#define MAX_FDS 16
#define STDIN_FD  0
#define STDOUT_FD 1
#define STDERR_FD 2

typedef enum {
    SCHED_STATE_OFF = 0,
    SCHED_STATE_READY = 1,
    SCHED_STATE_STARTING = 2,
    SCHED_STATE_RUNNING = 3,
} scheduler_state_t; 

typedef enum {
    PROCESS_STATE_READY = 0,
    PROCESS_STATE_SLEEPING = 1,
    PROCESS_STATE_ZOMBIE = 2
} process_state_t;

typedef struct process {
    const uint32_t pid;

    uint32_t pd_phys;
    
    uint32_t kernel_esp;
    uint32_t kernel_stack_top;
    uint32_t heap_brk;

    file_t* fds[MAX_FDS];
    uint32_t kernel_stack_base; 

    process_state_t state;
    timer_event_t* sleep_event;

    struct process* next;
} process_t;

void scheduler_init();
void scheduler_start();
void process_init(process_t* t, void (*entry)(void));
void process_exit(process_t* proc, interrupt_frame_t* frame);

void scheduler_add_process(process_t* proc);
process_t* scheduler_find_by_pid(uint32_t pid);

bool process_load_user_memory(process_t* p, uint32_t vaddr, const void* src, size_t len);

process_t* get_current_process(void);

void process_yield(void);
void process_sleep(uint32_t ticks);
void process_wake(process_t* proc);

#endif