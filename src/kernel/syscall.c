#include <kernel/syscall.h>
#include <lib/stdio.h>
#include <kernel/scheduler/scheduler.h>

// TODO: Add user pointer validation before dereferencing

typedef int (*syscall_func_t)(interrupt_frame_t* frame);


static int syscall_exit(interrupt_frame_t* frame) {
    process_t* current = get_current_process();
    printf("Process %d exiting with code %d\n", current->pid, frame->ebx);
    process_exit(current, frame);
    return SYSCALL_SUCCESS;
}

static int syscall_write(interrupt_frame_t* frame) {
    const char* buf = (const char*)frame->ecx;
    size_t len = frame->edx;

    for (size_t i = 0; i < len; i++)
        putc(buf[i]);
    return (int)len;
}


static int syscall_read(interrupt_frame_t* frame) {
    // TODO: implement this
    return 0;
}


static int syscall_sbrk(interrupt_frame_t* frame) {
    process_t* proc = get_current_process();
    int32_t incr = (int32_t)frame->ebx;

    if (incr == 0)
        return (int)proc->heap_brk;

    uint32_t old_brk = proc->heap_brk;
    uint32_t new_brk = old_brk + incr;

    if (incr > 0) {
        uint32_t old_pages = (old_brk - PROCESS_HEAP_START + PAGE_SIZE - 1) / PAGE_SIZE;
        uint32_t new_pages = (new_brk - PROCESS_HEAP_START + PAGE_SIZE - 1) / PAGE_SIZE;

        for (uint32_t i = old_pages; i < new_pages; i++) {
            uint32_t page = PROCESS_HEAP_START + i * PAGE_SIZE;
            if (!vmm_alloc_user_page_at(page)) {
                printf("sbrk: page alloc failed at 0x%x\n", page);
                return SYSCALL_ERROR;
            }
        }
    }

    proc->heap_brk = new_brk;
    return (int)old_brk;
}


static int syscall_close(interrupt_frame_t* frame) {
    // TODO: implement this
    return SYSCALL_SUCCESS;
}

static int syscall_fstat(interrupt_frame_t* frame) {
    // TODO: implement this
    return SYSCALL_SUCCESS;
}


// ebx=fd   returns 1 for stdin/stdout/stderr, 0 otherwise
static int syscall_isatty(interrupt_frame_t* frame) {
    uint32_t fd = frame->ebx;
    return (fd <= 2) ? 1 : 0;
}

// ebx=fd  ecx=offset  edx=whence   returns 0 (terminal, no seek)
static int syscall_lseek(interrupt_frame_t* frame) {
    // TODO: implement this
    return SYSCALL_SUCCESS;
}

static syscall_func_t sys_table[SYSCALL_COUNT] = {
    [SYSCALL_EXIT]   = syscall_exit,
    [SYSCALL_WRITE]  = syscall_write,
    [SYSCALL_READ]   = syscall_read,
    [SYSCALL_SBRK]   = syscall_sbrk,
    [SYSCALL_CLOSE]  = syscall_close,
    [SYSCALL_fSTAT]  = syscall_fstat,
    [SYSCALL_ISATTY] = syscall_isatty,
    [SYSCALL_LSEEK]  = syscall_lseek,
};

// SYSCALL HANDLER

void syscall_handler(interrupt_frame_t* frame) {
    // printf("%o[HD] syscall num %d\n", STD_COLOR_LIGHT_BLUE, frame->eax);
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