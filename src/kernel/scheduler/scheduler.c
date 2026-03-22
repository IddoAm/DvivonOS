#include <kernel/scheduler/scheduler.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <arch/i686/gdt.h>
#include <kernel/heap-allocator.h>
#include <fs/vfs/file.h>
#include <arch/i686/pic.h>

#define PROCESS_MAX_TICKS 5

static uint32_t next_pid = 1;

static process_t* current_process = NULL;
static process_t* process_list = NULL;
static volatile uint32_t current_process_ticks = 0;
static scheduler_state state = SCHED_STATE_OFF;

extern void switch_to_stack(uint32_t* old_esp, uint32_t new_esp);
extern void fork_ret(void);

// Must be called within an interrupt context
void context_switch(process_t* from, process_t* to) {
    // printf("%ofrom %d to %d", STD_COLOR_CYAN, from->pid, to->pid);
    current_process = to;
    tss_set_stack(to->kernel_stack_top);
    vmm_switch_address_space(to->pd_phys);
    switch_to_stack(&from->kernel_esp, to->kernel_esp);
}

void schedule(interrupt_frame_t* frame) {
    if (state == SCHED_STATE_STARTING) {
        state = SCHED_STATE_RUNNING;
        uint32_t dead_esp;
        tss_set_stack(current_process->kernel_stack_top);
        vmm_switch_address_space(current_process->pd_phys);
        switch_to_stack(&dead_esp, current_process->kernel_esp);
        return; // unreachable
    }

    if (state != SCHED_STATE_RUNNING || !current_process)
        return;

    current_process_ticks++;

    if (current_process_ticks >= PROCESS_MAX_TICKS) {
        current_process_ticks = 0;

        process_t* next_proc = current_process->next;
        if (!next_proc)
            next_proc = process_list;

        if (next_proc == current_process)
            return;
        context_switch(current_process, next_proc);
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
        process_t* last = process_list;
        while (last->next != process_list)
            last = last->next;

        last->next = proc;
        proc->next = process_list;
    }
}

void scheduler_start() {
    if (!process_list) {
        printf("scheduler_start: no processes to run\n");
        return;
    }

    current_process = process_list;
    state = SCHED_STATE_READY;
    
    vmm_switch_address_space(current_process->pd_phys);
    scheduler_init();
}

void process_init(process_t* p, void (*entry)(void)) {
    // Zero the process structure and set PID
    memset(p, 0, sizeof(process_t));
    *(uint32_t*)&p->pid = next_pid++;

    uint32_t kstack = kmalloc(PAGE_SIZE * 2);
    if (!kstack) {
        printf("Failed to allocate kernel stack\n");
        return;
    }
    p->kernel_stack_base = kstack;
    p->kernel_stack_top  = kstack + PAGE_SIZE * 2;

    p->pd_phys = vmm_create_address_space();

    uint32_t old_cr3 = vmm_read_cr3();
    vmm_switch_address_space(p->pd_phys);


    uint32_t stack_high = (PROCESS_STACK_TOP - 1) & ~(PAGE_SIZE - 1);
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        // uint32_t stack_page_vaddr = PROCESS_STACK_TOP - (i + 1) * PAGE_SIZE;
        // if (!vmm_alloc_user_page_at(stack_page_vaddr)) {
        uint32_t stack_page_vaddr = PROCESS_STACK_TOP - (i + 1) * PAGE_SIZE;
        if (!vmm_alloc_user_page_at(stack_page_vaddr)) {
            printf("process_init: failed to allocate user stack page\n");
            vmm_switch_address_space(old_cr3);
            return;
        }
    }
    vmm_switch_address_space(old_cr3);

    // Prime the kernel stack with an interrupt frame at the top
    interrupt_frame_t* frame = (interrupt_frame_t*)(p->kernel_stack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->eip   = (uint32_t)entry;
    frame->cs    = GDT_USER_CODE_SEL;
    frame->eflags = 0x202;
    frame->esp   = PROCESS_STACK_TOP;
    frame->ss    = GDT_USER_DATA_SEL;
    frame->ds    = frame->es = frame->fs = frame->gs = GDT_USER_DATA_SEL;

    // Build the stack that switch_to_stack expects when restoring this process:
    //   [edi] [esi] [ebx] [ebp] [return addr = fork_ret]
    // switch_to_stack will pop edi/esi/ebx/ebp, then ret into fork_ret
    uint32_t* stack_ptr = (uint32_t*)frame;
    stack_ptr--;
    *stack_ptr = (uint32_t)fork_ret;  // return address for ret
    stack_ptr--;
    *stack_ptr = 0;  // ebp
    stack_ptr--;
    *stack_ptr = 0;  // ebx
    stack_ptr--;
    *stack_ptr = 0;  // esi
    stack_ptr--;
    *stack_ptr = 0;  // edi

    p->kernel_esp = (uint32_t)stack_ptr;
    p->heap_brk = PROCESS_HEAP_START;
    frame->useresp = PROCESS_STACK_TOP; 


    p->next = NULL;
}

bool process_load_user_memory(process_t* p, uint32_t vaddr, const void* src, size_t len) {
    if (!p || !src || len == 0) return false;

    uint32_t old_cr3 = vmm_read_cr3();
    vmm_switch_address_space(p->pd_phys);

    uint32_t start = vaddr & ~(PAGE_SIZE - 1);
    uint32_t end   = (vaddr + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint32_t a = start; a < end; a += PAGE_SIZE) {
        if (!vmm_alloc_user_page_at(a)) {
            vmm_switch_address_space(old_cr3);
            return false;
        }
    }

    memcpy((void*)vaddr, src, len);
    vmm_switch_address_space(old_cr3);
    return true;
}

// Must be called within an interrupt context
void process_exit(process_t* proc, interrupt_frame_t* frame) {
    if (proc->next == proc) {
        printf("process_exit: last process exiting, halting system\n");
        for (;;) asm volatile("hlt");
    }

    // Unlink from circular list
    process_t* p = process_list;
    while (p->next != proc && p->next != process_list)
        p = p->next;

    if (p->next == proc) {
        p->next = proc->next;
        if (process_list == proc)
            process_list = proc->next;
    }

    // Close any open file descriptors
    for (int fd = STDERR_FD + 1; fd < MAX_FDS; fd++) {
        if (proc->fds[fd]) {
            file_close(proc->fds[fd]);
            proc->fds[fd] = NULL;
        }
    }

    // Switch away BEFORE freeing anything, using a throwaway save location
    if (proc == current_process) {
        process_t* next = process_list;
        process_t dead_proc;
        current_process = next;

        context_switch(&dead_proc, next);
        // Never reached — proc's memory is freed after we've already left its stack
    }

    // Only reached for non-current exiting processes
    vmm_destroy_address_space(proc->pd_phys);
    kfree((uintptr_t)proc->kernel_stack_base);
    kfree((uintptr_t)proc);
}

process_t* get_current_process(void) {
    return current_process;
}