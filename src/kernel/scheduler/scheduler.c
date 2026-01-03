#include <kernel/scheduler/scheduler.h>
#include <lib/stdio.h>
#include <lib/string.h>

#define TASK_MAX_TICKS 10

static volatile task_t* current_task = NULL;
static volatile process_t* current_process = NULL;
static volatile process_t* process_list = NULL;
static volatile uint32_t current_task_ticks = 0;
static scheduler_state state = SCHED_STATE_OFF;

void schedule(interrupt_frame_t* frame) {
    if (state == SCHED_STATE_STARTING) {
        state = SCHED_STATE_RUNNING;
        memcpy(frame, current_task->context, sizeof(interrupt_frame_t));
        return;
    }

    if (state != SCHED_STATE_RUNNING || !current_task)
        return;

    memcpy(current_task->context, frame, sizeof(interrupt_frame_t));
    current_task_ticks++;

    if (current_task_ticks >= TASK_MAX_TICKS) {
        current_task_ticks = 0;

        task_t* next_task = (task_t*)current_task->next;
        if (next_task) {
            current_task = next_task;
        } else {
            process_t* next_proc = (process_t*)current_process->next;
            if (!next_proc)
                next_proc = (process_t*)process_list;

            current_process = next_proc;
            current_task = current_process->task_list_head;

            vmm_switch_address_space(current_process->heap->page_dir);
        }

        memcpy(frame, current_task->context, sizeof(interrupt_frame_t));
    }
}

void scheduler_init() {
    if (state != SCHED_STATE_READY)
        return;

    isr_register_handler(irq_to_vector(0), (interrupt_handler_t)schedule);
    state = SCHED_STATE_STARTING;

    for (;;)
        asm volatile("hlt");
}

void scheduler_add_process(process_t* proc) {
    if (!process_list) {
        process_list = proc;
        proc->next = proc;
    } else {
        process_t* last = (process_t*)process_list;
        while (last->next != process_list)
            last = last->next;
        
        last->next = proc;
        proc->next = (process_t*)process_list;
    }
}

void scheduler_start() {
    if (!process_list) {
        printf("scheduler_start: no processes to run\n");
        return;
    }

    current_process = (process_t*)process_list;
    current_task = current_process->task_list_head;
    state = SCHED_STATE_READY;

    vmm_switch_address_space(current_process->heap->page_dir);
    scheduler_init();
}

void task_init(task_t* task, void (*entry)(void), uint32_t* stack_top) {
    interrupt_frame_t* frame =
        (interrupt_frame_t*)(stack_top - sizeof(interrupt_frame_t) / sizeof(uint32_t));
    memset(frame, 0, sizeof(*frame));

    frame->eip = (uint32_t)entry;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    frame->ds = frame->es = frame->fs = frame->gs = 0x10;

    frame->eax = frame->ecx = frame->edx = frame->ebx = 0;
    frame->esp = (uint32_t)stack_top;
    frame->ebp = frame->esi = frame->edi = 0;
    frame->int_no = 0;
    frame->err_code = 0;

    task->context = frame;
    task->entry = entry;
    task->state = 0;
    task->next = NULL;
}

void process_init(process_t* p, void (*entry)(void)) {
    page_directory_t* pd = vmm_create_address_space();
    p->heap = heap_create(PROCESS_HEAP_START, 8, 1024, pd);

    task_t* t = kmalloc(sizeof(task_t));

    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t stack_page_vaddr = PROCESS_STACK_TOP - (i + 1) * PAGE_SIZE;
        if (vmm_alloc_page_at(pd, stack_page_vaddr, PAGE_PRESENT | PAGE_RW | PAGE_USER) != 0) {
            printf("process_init: failed to allocate user stack page\n");
            return;
        }
    }

    task_init(t, entry, (uint32_t*)PROCESS_STACK_TOP);
    p->task_list_head = t;
    p->next = NULL;
}

task_t* get_current_task(void) {
    return (task_t*)current_task;
}

process_t* get_current_process(void) {
    return (process_t*)current_process;
}