#include <kernel/scheduler/scheduler.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <arch/i686/gdt.h>
#include <kernel/heap-allocator.h>
#include <fs/vfs/file.h>
#include <arch/i686/pic.h>
#include <stdbool.h>

#define PROCESS_MAX_TICKS 5

static uint32_t next_pid = 1;

static process_t* current_process = NULL;
static process_t* zombie_process = NULL;
static process_t* process_list = NULL;
static volatile uint32_t current_process_ticks = 0;
static scheduler_state_t state = SCHED_STATE_OFF;

extern void switch_to_stack(uint32_t* old_esp, uint32_t new_esp);
extern void fork_ret(void);

static void kernel_panic(const char* msg, interrupt_frame_t* frame) {
    printf("%o[KERNEL PANIC] %s\n", STD_COLOR_LIGHT_RED, msg);
    printf("  int=%d err=0x%x eip=0x%x cs=0x%x\n",
           frame->int_no, frame->err_code, frame->eip, frame->cs);
    for (;;) asm volatile("hlt");
}

void process_crash_handler(interrupt_frame_t* frame) {
    // Double fault and machine check always panic regardless of origin
    if (frame->int_no == 8 || frame->int_no == 18) {
        kernel_panic("fatal CPU exception", frame);
    }

    if ((frame->cs & 0x3) == 3) {
        process_t* proc = get_current_process();
        printf("%o[CRASH] process %d faulted: int=%d err=0x%x eip=0x%x\n",
               STD_COLOR_LIGHT_RED, proc->pid,
               frame->int_no, frame->err_code, frame->eip);
        process_exit(proc, frame);
    } else {
        kernel_panic("exception in kernel", frame);
    }
}

// Must be called within an interrupt context
void context_switch(process_t* from, process_t* to) {
    // printf("%ofrom %d to %d", STD_COLOR_CYAN, from->pid, to->pid);
    current_process = to;
    tss_set_stack(to->kernel_stack_top);
    vmm_switch_address_space(to->pd_phys);
    switch_to_stack(&from->kernel_esp, to->kernel_esp);
}

static process_t* find_next_ready(void) {
    process_t* curr = current_process;

    do {
        curr = curr->next;
        if (curr == NULL) {
            curr = process_list;
        }

        // Check second
        if (curr->state == PROCESS_STATE_READY) {
            return curr;
        }

    } while (curr != current_process);

    return NULL; 
}

void process_reap(process_t* proc){
    vmm_destroy_address_space(proc->pd_phys);
    kfree((uintptr_t)proc->kernel_stack_base);
    kfree((uintptr_t)proc);
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

    if(zombie_process) {
        process_reap(zombie_process);
        zombie_process = NULL;
    }

    if (state != SCHED_STATE_RUNNING || !current_process)
        return;

    current_process_ticks++;
    if (current_process_ticks >= PROCESS_MAX_TICKS) {
        current_process_ticks = 0;

        process_t* next_proc = find_next_ready();
        if(next_proc == NULL)
            printf("%ofound no next process", STD_COLOR_RED);
        if (next_proc == current_process)
            return;

        context_switch(current_process, next_proc);
    }
}

void scheduler_init() {
    if (state != SCHED_STATE_READY)
        return;

    isr_register_handler(irq_to_vector(0), (interrupt_handler_t)schedule);

    // Register crash handler for all fatal CPU exceptions (vectors 0-31, no irq_to_vector)
    isr_register_handler(0,  (interrupt_handler_t)process_crash_handler); // #DE divide by zero
    isr_register_handler(4,  (interrupt_handler_t)process_crash_handler); // #OF overflow
    isr_register_handler(5,  (interrupt_handler_t)process_crash_handler); // #BR bound range
    isr_register_handler(6,  (interrupt_handler_t)process_crash_handler); // #UD invalid opcode
    isr_register_handler(8,  (interrupt_handler_t)process_crash_handler); // #DF double fault
    isr_register_handler(10, (interrupt_handler_t)process_crash_handler); // #TS invalid TSS
    isr_register_handler(11, (interrupt_handler_t)process_crash_handler); // #NP segment not present
    isr_register_handler(12, (interrupt_handler_t)process_crash_handler); // #SS stack fault
    isr_register_handler(13, (interrupt_handler_t)process_crash_handler); // #GP general protection
    isr_register_handler(14, (interrupt_handler_t)process_crash_handler); // #PF page fault
    isr_register_handler(16, (interrupt_handler_t)process_crash_handler); // #MF x87 FPU error
    isr_register_handler(17, (interrupt_handler_t)process_crash_handler); // #AC alignment check
    isr_register_handler(18, (interrupt_handler_t)process_crash_handler); // #MC machine check
    isr_register_handler(19, (interrupt_handler_t)process_crash_handler); // #XF SIMD FP exception
    isr_register_handler(20, (interrupt_handler_t)process_crash_handler); // #VE virtualization

    state = SCHED_STATE_STARTING;
    
        for (;;)
        asm volatile("hlt");
}


process_t* scheduler_find_process(uint32_t pid) {
    if (!process_list) return NULL;
    process_t* p = process_list;
    do {
        if (p->pid == pid) return p;
        p = p->next;
    } while (p != process_list);
    return NULL;
}

process_t* scheduler_get_list(void) { return process_list; }

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

void process_init(process_t* p, void (*entry)(void),
                  int argc, const char** argv, const char* cwd) {
    // Zero the process structure and set PID
    memset(p, 0, sizeof(process_t));
    *(uint32_t*)&p->pid = next_pid++;

    p->state = PROCESS_STATE_READY;

    uint32_t kstack = kmalloc(PAGE_SIZE * 2);
    if (!kstack) {
        printf("Failed to allocate kernel stack\n");
        return;
    }
    p->kernel_stack_base = kstack;
    p->kernel_stack_top  = kstack + PAGE_SIZE * 2;

    // Store cwd BEFORE switching address space (cwd may point to caller's userspace)
    if (cwd) {
        strncpy(p->cwd, cwd, PROCESS_CWD_MAX - 1);
        p->cwd[PROCESS_CWD_MAX - 1] = '\0';
    } else {
        p->cwd[0] = '/';
        p->cwd[1] = '\0';
    }

    // argv[i] may point into the caller's userspace which disappears after the switch.
    if (argc > PROCESS_ARGV_MAX)
        printf("%o[WARN] process started with %d args but max is %d\n", STD_COLOR_LIGHT_RED, argc, PROCESS_ARGV_MAX);
    int safe_argc = (argc > PROCESS_ARGV_MAX) ? PROCESS_ARGV_MAX : argc;

    char* kargv[PROCESS_ARGV_MAX];
    for (int i = 0; i < PROCESS_ARGV_MAX; i++) kargv[i] = NULL;
    if (argv) {
        for (int i = 0; i < safe_argc; i++) {
            if (argv[i]) {
                uint32_t len = strlen(argv[i]) + 1;
                kargv[i] = (char*)kmalloc(len);
                if (kargv[i]) memcpy(kargv[i], argv[i], len);
            }
        }
    }

    p->pd_phys = vmm_create_address_space();

    uint32_t old_cr3 = vmm_read_cr3();
    vmm_switch_address_space(p->pd_phys);

    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t stack_page_vaddr = PROCESS_STACK_TOP - (i + 1) * PAGE_SIZE;
        if (!vmm_alloc_user_page_at(stack_page_vaddr)) {
            printf("process_init: failed to allocate user stack page\n");
            for (int j = 0; j < safe_argc; j++) if (kargv[j]) kfree((uintptr_t)kargv[j]);
            vmm_switch_address_space(old_cr3);
            return;
        }
    }

    // Build argc/argv layout on the user stack using the kernel-heap copies
    uint32_t sp = PROCESS_STACK_TOP;
    uint32_t argv_ptrs[PROCESS_ARGV_MAX];

    for (int i = safe_argc - 1; i >= 0; i--) {
        const char* src = kargv[i] ? kargv[i] : "";
        uint32_t len = strlen(src) + 1;
        sp -= len;
        memcpy((void*)sp, src, len);
        argv_ptrs[i] = sp;
    }

    sp &= ~3U; // align to 4 bytes

    // Push NULL sentinel (end of argv[])
    sp -= 4;
    *(uint32_t*)sp = 0;
    // Push argv pointers
    for (int i = safe_argc - 1; i >= 0; i--) {
        sp -= 4; *(uint32_t*)sp = argv_ptrs[i];
    }
    // Push argc
    sp -= 4;
    *(uint32_t*)sp = (uint32_t)safe_argc;

    vmm_switch_address_space(old_cr3);

    // Free kernel-side argv copies
    for (int i = 0; i < safe_argc; i++) if (kargv[i]) kfree((uintptr_t)kargv[i]);

    // Prime the kernel stack with an interrupt frame at the top
    interrupt_frame_t* frame = (interrupt_frame_t*)(p->kernel_stack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t));

    frame->eip    = (uint32_t)entry;
    frame->cs     = GDT_USER_CODE_SEL;
    frame->eflags = 0x202;
    frame->esp    = sp;
    frame->ss     = GDT_USER_DATA_SEL;
    frame->ds     = frame->es = frame->fs = frame->gs = GDT_USER_DATA_SEL;
    frame->useresp = sp;

    // Build the stack that switch_to_stack expects when restoring this process:
    //   [edi] [esi] [ebx] [ebp] [return addr = fork_ret]
    uint32_t* stack_ptr = (uint32_t*)frame;
    stack_ptr--;
    *stack_ptr = (uint32_t)fork_ret;
    stack_ptr--;
    *stack_ptr = 0;  // ebp
    stack_ptr--;
    *stack_ptr = 0;  // ebx
    stack_ptr--;
    *stack_ptr = 0;  // esi
    stack_ptr--;
    *stack_ptr = 0;  // edi

    p->kernel_esp = (uint32_t)stack_ptr;
    p->brk_pointer = PROCESS_HEAP_START;
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

    if(proc->state == PROCESS_STATE_SLEEPING) {
        unregister_timer_event(proc->sleep_event);
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

    // Unblock any process that was waiting for this one
    if (process_list) {
        process_t* p = process_list;
        do {
            if (p->waiting_for_pid == proc->pid) {
                p->state = PROCESS_STATE_READY;
                p->waiting_for_pid = 0;
            }
            p = p->next;
        } while (p && p != process_list);
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
        // Incase there's already a zombie process
        if(zombie_process){
            process_reap(zombie_process);
        }

        proc->state = PROCESS_STATE_ZOMBIE;
        zombie_process = proc;

        process_t* next = find_next_ready();
        process_t dead_proc;
        current_process = next;

        context_switch(&dead_proc, next);
        // Can't reach
    }

    // Only reached for non-current exiting processes
    process_reap(proc);
}

void process_yield(void) {
    current_process_ticks = PROCESS_MAX_TICKS;
    
    // Trigger your timer interrupt vector. 
    asm volatile("int $0x20"); 
}

void process_wake(process_t* proc) {
    if (!proc) return;

    // If there was a pending timer/sleep event, cancel it now
    if (proc->sleep_event != NULL) {
        unregister_timer_event(proc->sleep_event);
        proc->sleep_event = NULL;
    }

    proc->state = PROCESS_STATE_READY;
}

void process_wake_callback(void* data) {
    if (!data) return;
    process_t* proc = (process_t*)data;

    proc->state = PROCESS_STATE_READY;
    proc->sleep_event = NULL;
}

void process_sleep(uint32_t ticks) {
    if (!current_process) return;

    current_process->state = PROCESS_STATE_SLEEPING;
    current_process->sleep_event = register_timer_event(ticks, process_wake_callback, (void*)current_process);
    process_yield();
}

process_t* get_current_process(void) {
    return current_process;
}