#include <kernel/syscall.h>

#include <arch/i686/idt.h>
#include <arch/i686/idt.h>
#include <arch/i686/irq_lock.h>

#include <lib/stdio.h>

#include <kernel/scheduler/scheduler.h>

// TODO: Add user pointer check and copy to local buffer for safety


typedef int (*syscall_func_t)(interrupt_frame_t* frame);

// SYSCALL FUNCTIONS

static int  syscall_exit(interrupt_frame_t* frame) {
    process_t* current = get_current_process();
    printf("Process %d exiting with code %d\n", current->pid, frame->ebx);
    process_exit(current, frame);
    return SYSCALL_SUCCESS; // Unreachable
}

static int  syscall_write(interrupt_frame_t* frame) {
    // Ignore file descriptor for now (in ebx)
    interrupt_lock_t lock;
    
    for (size_t i = 0; i < frame->edx; i++) {
        // Make the putc operation atomic
        lock_interrupts(&lock);

        putc(((const char*)frame->ecx)[i]);

        unlock_interrupts(&lock);
    }
    
    return SYSCALL_SUCCESS;
}
 

// SYSCALL TABLE

static syscall_func_t sys_table[SYSCALL_COUNT] = {
    syscall_exit,
    syscall_write
};

// SYSCALL HANDLER

void syscall_handler(interrupt_frame_t* frame) {
    uint32_t num = frame->eax;
    int ret = 1;

    if (num < SYSCALL_COUNT && sys_table[num]) {
        ret = sys_table[num](frame);
    } else {
        printf("syscall: unknown syscall %d\n", num);
    }

    frame->eax = (uint32_t)ret;
}


void syscall_init() {
    isr_register_handler(SYSCALL_INT, syscall_handler);
}