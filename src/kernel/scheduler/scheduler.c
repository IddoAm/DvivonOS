#include <kernel/scheduler/scheduler.h>
#include <lib/stdio.h>
#include <lib/string.h> // for memset


#define TASK_MAX_TICKS 10

static volatile task_t* task_list_head = 0;
static volatile task_t* current_task = 0;
static volatile uint32_t current_task_ticks = 0;

static scheduler_state_t state = SCHED_STATE_OFF;

void schedule(interrupt_frame_t* frame) {
    if(state == SCHED_STATE_STARTING) {
        state = SCHED_STATE_RUNNING;
        memcpy(frame, current_task->context, sizeof(interrupt_frame_t));
        return;
    }

    if (state != SCHED_STATE_RUNNING || !current_task || !current_task->next) return;

    // save current task registers
    memcpy(current_task->context, frame, sizeof(interrupt_frame_t));

    current_task_ticks++;
    if (current_task_ticks >= TASK_MAX_TICKS) {
        current_task_ticks = 0;

       // printf("\n");
        // pick next task
        current_task = current_task->next;

        // overwrite IRQ frame with next task's context
        memcpy(frame, current_task->context, sizeof(interrupt_frame_t));
    }
}


void scheduler_init() {
    if(state != SCHED_STATE_READY) return;

    isr_register_handler(irq_to_vector(0), (interrupt_handler_t)schedule);
    state = SCHED_STATE_STARTING;
    for (;;) asm volatile("hlt"); 
}

// Initialize a single task
void task_init(task_t* task, void (*entry)(void), uint32_t* stack_top) {
    interrupt_frame_t* frame = (interrupt_frame_t*)(stack_top - sizeof(interrupt_frame_t)/sizeof(uint32_t));
    memset(frame, 0, sizeof(*frame));

    // Set CPU context
    frame->eip = (uint32_t)entry;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    frame->ds = frame->es = frame->fs = frame->gs = 0x10;

    // push dummy general purpose registers (popa order)
    frame->eax = frame->ecx = frame->edx = frame->ebx = 0;
    frame->esp = (uint32_t)stack_top;   // original stack pointer
    frame->ebp = frame->esi = frame->edi = 0;

    frame->int_no = 0;
    frame->err_code = 0;

    task->context = frame;
    task->entry = entry;
    task->state = 0; // READY

    // add to circular list
    if (!task_list_head) {
        task_list_head = task;
        task->next = task;
        current_task = task;
        state = SCHED_STATE_READY;
    } else {
        task_t* tail = task_list_head;
        while (tail->next != task_list_head)
            tail = tail->next;
        tail->next = task;
        task->next = task_list_head;
    }
}

