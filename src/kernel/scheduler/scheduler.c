#include <kernel/scheduler/scheduler.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <arch/i686/gdt.h>

#define PROCESS_MAX_TICKS 10

static volatile process_t* current_process = NULL;
static volatile process_t* process_list = NULL;
static volatile uint32_t current_process_ticks = 0;
static scheduler_state state = SCHED_STATE_OFF;

// Must be called within an interrupt context
void context_switch(process_t* from, process_t* to, interrupt_frame_t* frame) {
    if (from) {
        memcpy(from->context, frame, sizeof(interrupt_frame_t));
    }
    
    // Switch to new process
    current_process = to;
    current_process_ticks = 0;
    vmm_switch_address_space(to->heap->page_dir);
    
    // Load incoming process state
    memcpy(frame, to->context, sizeof(interrupt_frame_t));
}

void schedule(interrupt_frame_t* frame) {
    if (state == SCHED_STATE_STARTING) {
        state = SCHED_STATE_RUNNING;
        memcpy(frame, current_process->context, sizeof(interrupt_frame_t));
        return;
    }

    if (state != SCHED_STATE_RUNNING || !current_process)
        return;

    current_process_ticks++;

    // Time to switch?
    if (current_process_ticks >= PROCESS_MAX_TICKS) {
        current_process_ticks = 0;

        // Move to next process
        process_t* next_proc = (process_t*)current_process->next;
        if (!next_proc)
            next_proc = (process_t*)process_list;

        context_switch(current_process, next_proc, frame);
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
    state = SCHED_STATE_READY;

    vmm_switch_address_space(current_process->heap->page_dir);
    scheduler_init();
}

void process_init(process_t* p, void (*entry)(void)) {
    // To set const value
    *(uint32_t*)&p->pid = next_pid++;

    page_directory_t* pd = vmm_create_address_space();
    p->heap = heap_create(PROCESS_HEAP_START, 8, 1024, pd);

    // Allocate user stack
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t stack_page_vaddr = PROCESS_STACK_TOP - (i + 1) * PAGE_SIZE;
        if (vmm_alloc_page_at(pd, stack_page_vaddr, PAGE_PRESENT | PAGE_RW | PAGE_USER) != 0) {
            printf("process_init: failed to allocate user stack page\n");
            return;
        }
    }

    // Set up initial context
    interrupt_frame_t* frame = kmalloc(sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(*frame));

    frame->eip = (uint32_t)entry;
    frame->cs = GDT_USER_CODE_SEL;
    frame->eflags = 0x202;
    frame->ss = frame->ds = frame->es = frame->fs = frame->gs = GDT_USER_DATA_SEL;

    frame->eax = frame->ecx = frame->edx = frame->ebx = 0;
    frame->esp = PROCESS_STACK_TOP;
    frame->ebp = frame->esi = frame->edi = 0;
    frame->int_no = 0;
    frame->err_code = 0;

    p->context = frame;
    p->next = NULL;
}

// Must be called within an interrupt context
void process_exit(process_t* proc, interrupt_frame_t* frame) {
    // Can't exit if no other processes exist
    if (proc->next == proc) {
        printf("process_exit: last process exiting, halting system\n");
        for(;;) asm volatile("hlt");
    }
    
    // Remove from process list
    process_t* p = (process_t*)process_list;
    while (p->next != proc && p->next != process_list) {
        p = p->next;
    }
    
    if (p->next == proc) {
        p->next = proc->next;
        if (process_list == proc) {
            process_list = proc->next;
        }
    }
    
    // Free context
    kfree(proc->context);
    
    // Free resources
    // IMPORTANT TODO:
    // AFTER VMM REFACTOR DESTROY PROCESS ADDRESS SPACE HERE
    
    kfree(proc);
    
    // If current process is exiting, force immediate reschedule
    if (proc == current_process) {
        process_t* next = (process_t*)process_list;
        context_switch(NULL, next, frame);  // Don't save dying process state
    }
}

process_t* get_current_process(void) {
    return (process_t*)current_process;
}