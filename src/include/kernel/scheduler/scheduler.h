#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <arch/i686/idt.h>
#include <fs/vfs/file.h>
#include <kernel/heap-allocator.h>
#include <kernel/vmm.h>
#include <stdbool.h>
#include <stdint.h>

#define PROCESS_HEAP_START 0x00601000

#define PROCESS_STACK_TOP 0xBFF00000
#define USER_STACK_PAGES 4

#define MAX_FDS 16
#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

#define PROCESS_NAME_MAX 64
#define PROCESS_CWD_MAX 256
#define PROCESS_ARGV_MAX 32

typedef enum {
    SCHED_STATE_OFF = 0,
    SCHED_STATE_READY = 1,
    SCHED_STATE_STARTING = 2,
    SCHED_STATE_RUNNING = 3,
} scheduler_state; // TODO: think if it need to end with _t

typedef struct process {
    const uint32_t pid;
    uint32_t parent_pid;         // 0 = no parent (init)
    char name[PROCESS_NAME_MAX]; // basename of ELF, e.g. "terminal"
    char cwd[PROCESS_CWD_MAX];   // current working directory

    uint32_t pd_phys;

    uint32_t kernel_esp;
    uint32_t kernel_stack_top;
    uint32_t heap_brk;

    file_t* fds[MAX_FDS];
    uint32_t kernel_stack_base;

    uint32_t waiting_for_pid; // 0 = not blocked; >0 = waiting for this PID
    bool blocked;             // true while waiting for a child

    struct process* next;
} process_t;

void scheduler_init();
void scheduler_start();
void process_init(process_t* p, void (*entry)(void), int argc, const char** argv, const char* cwd);
void process_exit(process_t* proc, interrupt_frame_t* frame);

void scheduler_add_process(process_t* proc);
bool process_load_user_memory(process_t* p, uint32_t vaddr, const void* src, size_t len);

process_t* get_current_process(void);
process_t* scheduler_find_process(uint32_t pid);
void process_yield(void);
process_t* scheduler_get_list(void);

#endif