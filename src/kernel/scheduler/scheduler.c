#include <kernel/scheduler/scheduler.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <arch/i686/gdt.h>
#include <kernel/heap-allocator.h>

#define PROCESS_MAX_TICKS 100

static uint32_t next_pid = 1;

static process_t* current_process = NULL;
static process_t* process_list = NULL;
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
    vmm_switch_address_space(to->pd_phys);

    // printf("[DBG] context_switch to pid=%d eip=0x%x\n", (int)to->pid, (unsigned)to->context->eip);

    // set kernel stack
    tss_set_stack(to->kernel_stack_top);
    
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
    
    vmm_switch_address_space(current_process->pd_phys);
    scheduler_init();
}

void process_init(process_t* p, void (*entry)(void)) {
    // To set const value
    *(uint32_t*)&p->pid = next_pid++;

    uint32_t kstack = kmalloc(PAGE_SIZE);
    if (!kstack) {
        printf("Failed to allocate kernel stack\n");
        return;
    }
    p->kernel_stack_top = kstack + PAGE_SIZE;

    p->pd_phys = vmm_create_address_space();
    // Allocate user stack - allocate while switched to the new address space
    uint32_t old_cr3 = vmm_read_cr3();
    vmm_switch_address_space(p->pd_phys);


    uint32_t stack_high = (PROCESS_STACK_TOP - 1) & ~(PAGE_SIZE - 1);
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t stack_page_vaddr = stack_high - i * PAGE_SIZE;
        if (vmm_alloc_user_page_at(stack_page_vaddr) == false) {
            printf("process_init: failed to allocate user stack page\n");
            vmm_switch_address_space(old_cr3);
            return;
        }
    }
    // restore original address space
    vmm_switch_address_space(old_cr3);
    // Set up initial context
    interrupt_frame_t* frame = (interrupt_frame_t*)kmalloc(sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(*frame));
    
    frame->eip = (uint32_t)entry;
    frame->cs = GDT_USER_CODE_SEL; // Ensure this is 0x1B (Index 3 | Ring 3)
    frame->eflags = 0x202;         // Interrupts Enabled
    
    // Data segments
    frame->ss = frame->ds = frame->es = frame->fs = frame->gs = GDT_USER_DATA_SEL; // Ensure 0x23

    frame->useresp = PROCESS_STACK_TOP; // User ESP for iret ring-3 return
    
    p->context = frame;
    p->heap_brk = PROCESS_HEAP_START;
    p->next = NULL;
}

// Load a blob (code/data) into a process virtual address.
// Returns true on success.
bool process_load_user_memory(process_t* p, uint32_t vaddr, const void* src, size_t len) {
    if (!p || !src || len == 0) return false;

    uint32_t old_cr3 = vmm_read_cr3();
    vmm_switch_address_space(p->pd_phys);

    uint32_t start = vaddr & ~(PAGE_SIZE - 1);
    uint32_t end = (vaddr + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // allocate pages covering the range
    for (uint32_t a = start; a < end; a += PAGE_SIZE) {
        if (!vmm_alloc_user_page_at(a)) {
            // restore and fail
            vmm_switch_address_space(old_cr3);
            return false;
        }
    }

    // copy the data into user virtual memory (we are currently in that PD)
    memcpy((void*)vaddr, src, len);

    // restore original address space
    vmm_switch_address_space(old_cr3);
    return true;
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
    kfree((uintptr_t)proc->context);

    // Free resources
    vmm_destroy_address_space(proc->pd_phys);
    kfree((uintptr_t)proc);
    kfree((uintptr_t)proc->kernel_stack_top);

    // If current process is exiting, force immediate reschedule
    if (proc == current_process) {
        process_t* next = (process_t*)process_list;
        context_switch(NULL, next, frame);  // Don't save dying process state
    }
}

process_t* get_current_process(void) {
    return (process_t*)current_process;
}