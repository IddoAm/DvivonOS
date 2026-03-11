#include <kernel/scheduler/scheduler.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <arch/i686/gdt.h>
#include <kernel/heap-allocator.h>
#include <arch/i686/pic.h>

#define PROCESS_MAX_TICKS 2

static uint32_t next_pid = 1;

static process_t* current_process = NULL;
static process_t* process_list = NULL;
static volatile uint32_t current_process_ticks = 0;
static scheduler_state state = SCHED_STATE_OFF;

extern void switch_to_stack(uint32_t* old_esp, uint32_t new_esp);
extern void fork_ret(void);

// Must be called within an interrupt context
void context_switch(process_t* from, process_t* to, interrupt_frame_t* frame) {
    printf("SWITCH\n");
// 1. Prepare global state and hardware
    current_process = to;
    tss_set_stack(to->kernel_stack_top);
    vmm_switch_address_space(to->pd_phys);
    // 2. Perform the swap
    // After this line, the CPU is executing Process 'to'
    switch_to_stack(&from->kernel_esp, to->kernel_esp);
}

void schedule(interrupt_frame_t* frame) {

    outb(0x20, 0x20);

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

    uint32_t kstack = kmalloc(PAGE_SIZE*8);
    if (!kstack) {
        printf("Failed to allocate kernel stack\n");
        return;
    }
    p->kernel_stack_base = kstack;               // store base
    p->kernel_stack_top = kstack + PAGE_SIZE*8;    // store top

    p->pd_phys = vmm_create_address_space();
    // Allocate user stack - allocate while switched to the new address space
    uint32_t old_cr3 = vmm_read_cr3();
    vmm_switch_address_space(p->pd_phys);

    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t stack_page_vaddr = PROCESS_STACK_TOP - (i + 1) * PAGE_SIZE;
        if (vmm_alloc_user_page_at(stack_page_vaddr) == false) {
            printf("process_init: failed to allocate user stack page\n");
            // restore old cr3 before returning
            vmm_switch_address_space(old_cr3);
            return;
        }
    }
    // restore original address space
    vmm_switch_address_space(old_cr3);
    // Set up initial context
// 3. PRIME THE KERNEL STACK
    // Place the frame at the very top of the stack
    interrupt_frame_t* frame = (interrupt_frame_t*)(p->kernel_stack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    // Initial User State
    frame->eip = (uint32_t)entry;
    frame->cs = GDT_USER_CODE_SEL; // 0x1B
    frame->eflags = 0x202;         // IF set
    frame->esp = PROCESS_STACK_TOP; 
    frame->ss = GDT_USER_DATA_SEL; // 0x23
    frame->ds = frame->es = frame->fs = frame->gs = GDT_USER_DATA_SEL;

    // 4. SET THE BOOKMARK (kernel_esp)
    // We need to simulate a function call. 
    // We put the address of 'fork_ret' below the frame.
    uint32_t* stack_ptr = (uint32_t*)frame;
    stack_ptr--; // Move down 4 bytes
    *stack_ptr = (uint32_t)fork_ret; // This is what switch_to_stack's 'ret' will hit

    p->kernel_esp = (uint32_t)stack_ptr;
    
    p->context = frame;
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
    
    // If current process is exiting, force immediate reschedule BEFORE freeing proc
    if (proc == current_process) {
        process_t* next = (process_t*)process_list;
        context_switch(NULL, next, frame);  // Don't save dying process state
    }

    // Free context
    kfree((uintptr_t)proc->context);

    // Free resources
    vmm_destroy_address_space(proc->pd_phys);

    // Free kernel stack using the base pointer we stored
    kfree((uintptr_t)proc->kernel_stack_base);

    // Finally free process struct itself
    kfree((uintptr_t)proc);
}

process_t* get_current_process(void) {
    return (process_t*)current_process;
}