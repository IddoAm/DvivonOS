#include <kernel/syscall.h>
#include <lib/stdio.h>
#include <kernel/scheduler/scheduler.h>
#include <fs/vfs/file.h>
#include <drivers/keyboard.h>
#include <arch/i686/idt.h>
#include <arch/i686/irq_lock.h>

// TODO: Add user pointer validation before dereferencing

typedef int (*syscall_func_t)(interrupt_frame_t* frame);

static int _process_alloc_fd(process_t* proc, file_t* file) {
    if (!proc || !file) return -1;
    for (int fd = STDERR_FD + 1; fd < MAX_FDS; fd++) {
        if (!proc->fds[fd]) {
            proc->fds[fd] = file;
            return fd;
        }
    }
    return -1;
}

static file_t* _process_get_fd(process_t* proc, int fd) {
    if (!proc) return NULL;
    if (fd <= STDERR_FD || fd >= MAX_FDS) return NULL;
    return proc->fds[fd];
}

static int syscall_exit(interrupt_frame_t* frame) {
    process_t* current = get_current_process();
    printf("Process %d exiting with code %d\n", current->pid, frame->ebx);
    process_exit(current, frame);
    return SYSCALL_SUCCESS;
}

static int syscall_write(interrupt_frame_t* frame) {
    uint32_t fd = frame->ebx;
    const char* buf = (const char*)frame->ecx;
    size_t len = frame->edx;

    interrupt_lock_t lock;

    if (!buf)
        return SYSCALL_ERROR;

    if (len == 0)
        return 0;

    if (fd == STDOUT_FD || fd == STDERR_FD) {
        for (size_t i = 0; i < len; i++)
        {
            lock_interrupts(&lock);
            putc(buf[i]);
            unlock_interrupts(&lock);
        }
        return (int)len;
    }

    file_t* f = _process_get_fd(get_current_process(), fd);
    if (!f) return SYSCALL_ERROR;

    return file_write(f, buf, len);
}

static int syscall_read(interrupt_frame_t* frame) {
    uint32_t fd = frame->ebx;
    char* buf = (char*)frame->ecx;
    size_t len = frame->edx;

    if (!buf)
        return SYSCALL_ERROR;

    if (len == 0)
        return 0;

    if (fd == STDIN_FD) {
        size_t i = 0;
        while (i < len) {
            key_event ev;
            while (!keyboard_read(&ev)) {
                __asm__ volatile("hlt");
            }
            if (!ev.pressed)
                continue;
            if (ev.ascii == 0)
            {
                if (ev.code == KC_BSPC && i > 0) {
                    buf[--i] = '\0';
                    printf("\b \b");
                }
                continue;
            }

            buf[i++] = (char)ev.ascii;
            putc(ev.ascii);
            if (ev.ascii == '\n')
                break;
        }
        return (int)i;
    }

    file_t* f = _process_get_fd(get_current_process(), fd);
    if (!f) return SYSCALL_ERROR;

    return file_read(f, buf, len);
}


static int syscall_open(interrupt_frame_t* frame) {
    const char* path = (const char*)frame->ebx;
    uint32_t flags = frame->ecx;

    if (!path)
        return SYSCALL_ERROR;

    uint32_t file_flags = 0;
    uint32_t acc = flags & 3;
    if (acc == 0)
        file_flags |= FILE_FLAG_READ;
    else if (acc == 1)
        file_flags |= FILE_FLAG_WRITE;
    else if (acc == 2)
        file_flags |= (FILE_FLAG_READ | FILE_FLAG_WRITE);

    // Map append behavior
    if (flags & 0x400)
        file_flags |= FILE_FLAG_APPEND;

    file_t* f = file_open(path, file_flags);
    if (!f)
        return SYSCALL_ERROR;

    int fd = _process_alloc_fd(get_current_process(), f);
    if (fd < 0) {
        file_close(f);
        return SYSCALL_ERROR;
    }

    return fd;
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
    int fd = (int)frame->ebx;
    process_t* proc = get_current_process();

    if (fd <= STDERR_FD || fd >= MAX_FDS)
        return SYSCALL_ERROR;

    file_t* f = proc->fds[fd];
    if (!f)
        return SYSCALL_ERROR;

    proc->fds[fd] = NULL;
    return file_close(f) == 0 ? SYSCALL_SUCCESS : SYSCALL_ERROR;
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
    [SYSCALL_OPEN]   = syscall_open,
    [SYSCALL_SBRK]   = syscall_sbrk,
    [SYSCALL_CLOSE]  = syscall_close,
    [SYSCALL_FSTAT]  = syscall_fstat,
    [SYSCALL_ISATTY] = syscall_isatty,
    [SYSCALL_LSEEK]  = syscall_lseek,
};

// SYSCALL HANDLER

void syscall_handler(interrupt_frame_t* frame) {
    printf("%o[HD] syscall num %d\n", STD_COLOR_LIGHT_BLUE, frame->eax);
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