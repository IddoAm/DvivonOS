#include <kernel/scheduler/scheduler.h>
#include <kernel/time/time.h>
#include <lib/stdio.h>

#define TASK_MAX_TICKS 5

volatile task_t* task_list_head = 0;
volatile task_t* current_task = 0;
volatile uint32_t current_task_ticks = 0;

// Preemptive scheduler called on each timer tick
void schedule(uint64_t ticks) {
    if (!current_task || !current_task->next) return;
    printf("%d\n", current_task_ticks);
    current_task_ticks++;
    if (current_task_ticks >= TASK_MAX_TICKS) {
        current_task_ticks = 0;

        // Save current ESP into currecnt task
        asm volatile("mov %%esp, %0" : "=r"(current_task->esp));

        // Pick next task
        current_task = current_task->next;

        // Load ESP from next task
        asm volatile("mov %0, %%esp" : : "r"(current_task->esp));
    }
}

// Initialize scheduler
void scheduler_init() {
    register_timer_callback(schedule);
}

// Initialize a single task
void task_init(task_t* task, void (*entry)(void), uint32_t* stack_top) {
    task->entry = entry;
    task->esp = stack_top;

    // Prepare stack as if it was interrupted
    // Stack layout: [pusha regs][EIP][CS][EFLAGS]
    *(--task->esp) = 0x202;           // EFLAGS (IF=1)
    *(--task->esp) = 0x08;            // CS (kernel code segment)
    *(--task->esp) = (uint32_t)entry; // EIP

    // Push dummy general-purpose registers for pusha
    for (int i = 0; i < 8; i++)
        *(--task->esp) = 0;

    task->state = 0; // READY

    // Add to circular task list
    if (!task_list_head) {
        task_list_head = task;
        task->next = task;
        current_task = task;
    } else {
        task->next = task_list_head->next;
        task_list_head->next = task;
    }
}

// Do an initial context switch to the first task
void start_first_task() {
    if (!current_task) return;

    asm volatile(
        "mov %0, %%esp\n"  // Load ESP
        "popa\n"           // Pop registers
        "iret\n"           // Return to task
        :
        : "r"(current_task->esp)
    );
}