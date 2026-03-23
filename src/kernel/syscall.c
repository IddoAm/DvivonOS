#include <arch/i686/idt.h>
#include <arch/i686/irq_lock.h>
#include <drivers/keyboard.h>
#include <fs/vfs/file.h>
#include <fs/vfs/inode.h>
#include <kernel/elf.h>
#include <kernel/heap-allocator.h>
#include <kernel/scheduler/scheduler.h>
#include <kernel/syscall.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <lib/fs.h>

// TODO: Add user pointer validation before dereferencing

typedef int (*syscall_func_t)(interrupt_frame_t* frame);

static int _process_alloc_fd(process_t* proc, file_t* file) {
    if (!proc || !file)
        return -1;
    for (int fd = STDERR_FD + 1; fd < MAX_FDS; fd++) {
        if (!proc->fds[fd]) {
            proc->fds[fd] = file;
            return fd;
        }
    }
    return -1;
}

static file_t* _process_get_fd(process_t* proc, int fd) {
    if (!proc)
        return NULL;
    if (fd <= STDERR_FD || fd >= MAX_FDS)
        return NULL;
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
        for (size_t i = 0; i < len; i++) {
            lock_interrupts(&lock);
            putc(buf[i]);
            unlock_interrupts(&lock);
        }
        return (int)len;
    }

    file_t* f = _process_get_fd(get_current_process(), fd);
    if (!f)
        return SYSCALL_ERROR;

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
        int i = 0;
        while (i < len) {
            key_event ev;
            while (!keyboard_read(&ev)) {
                __asm__ volatile("hlt");
            }
            if (!ev.pressed)
                continue;

            if (ev.ascii == 0)
                continue;

            if (ev.code == KC_BSPC) {
                if (i > 0) {
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
    if (!f)
        return SYSCALL_ERROR;

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

// ebx=fd  ecx=offset  edx=whence
static int syscall_lseek(interrupt_frame_t* frame) {
    int fd          = (int)frame->ebx;
    int32_t offset  = (int32_t)frame->ecx;
    int whence      = (int)frame->edx;

    file_t* f = _process_get_fd(get_current_process(), fd);
    if (!f) return SYSCALL_ERROR;

    uint32_t size = f->inode ? f->inode->size : 0;
    switch (whence) {
        case 0: f->offset = (uint32_t)offset; break;                            // SEEK_SET
        case 1: f->offset = (uint32_t)((int32_t)f->offset + offset); break;     // SEEK_CUR
        case 2: f->offset = (uint32_t)((int32_t)size + offset); break;          // SEEK_END
        default: return SYSCALL_ERROR;
    }
    return (int)f->offset;
}

// ebx=pid
static int syscall_kill(interrupt_frame_t* frame) {
    uint32_t pid = (uint32_t)frame->ebx;
    process_t* cur = get_current_process();
    if (cur->pid == pid) {
        process_exit(cur, frame);   // never returns
        return SYSCALL_SUCCESS;     // unreachable
    }
    process_t* target = scheduler_find_process(pid);
    if (!target) return SYSCALL_ERROR;
    process_exit(target, frame);    // non-current: frees memory, no context switch
    return SYSCALL_SUCCESS;
}

// Kernel-side mirror of userspace proc_create_args_t (must stay in sync with syscalls.h)
typedef struct {
    const char*  path;
    int          argc;
    const char** argv;
    const char*  cwd;
} proc_create_args_t;

// ebx=pointer to proc_create_args_t   returns new PID or -1
static int syscall_create_proc(interrupt_frame_t* frame) {
    const proc_create_args_t* uargs = (const proc_create_args_t*)frame->ebx;
    if (!uargs || !uargs->path) return SYSCALL_ERROR;

    file_t* f = file_open(uargs->path, FILE_FLAG_READ);
    if (!f) return SYSCALL_ERROR;

    uint32_t size = f->inode ? f->inode->size : 0;
    if (size == 0) { file_close(f); return SYSCALL_ERROR; }

    void* buf = (void*)kmalloc(size);
    if (!buf) { file_close(f); return SYSCALL_ERROR; }

    file_read(f, buf, size);
    file_close(f);

    process_t* p = load_elf(buf, size,
                            uargs->argc,
                            (const char**)uargs->argv,
                            uargs->cwd ? uargs->cwd : "/");
    kfree((uintptr_t)buf);
    if (!p) return SYSCALL_ERROR;

    p->parent_pid = get_current_process()->pid;

    // Set process name from basename of path
    const char* name = uargs->path;
    int name_len = strlen(name);
    if (name_len > 1 && uargs->path[0] == '/')
    {
        int start = name_len - 1;
        while (name[start] != '/') start--;
        name += start + 1;
    }
    strncpy(p->name, name, PROCESS_NAME_MAX - 1);

    scheduler_add_process(p);
    return (int)p->pid;
}



// ebx=uint32_t* buf   ecx=max_entries   returns count written or -1
static int syscall_ps(interrupt_frame_t* frame) {
    uint32_t* buf = (uint32_t*)frame->ebx;
    int max = (int)frame->ecx;
    if (!buf || max <= 0) return SYSCALL_ERROR;

    int count = 0;
    process_t* list = scheduler_get_list();
    if (!list) return 0;

    process_t* p = list;
    if (p == NULL) return 0;

    do {
        if (count >= max) break;
        buf[count] = p->pid;
    } while (p && p != list);

    return count;
}

// Kernel-side proc_stat_t (must stay in sync with userspace proc_stat_t in syscalls.h)
typedef struct {
    int  pid;
    int  parent_pid;
    char name[PROCESS_NAME_MAX];
    char cwd[PROCESS_CWD_MAX];
} proc_stat_t;

// ebx=pid (0=current)   ecx=proc_stat_t*   returns 0 or -1
static int syscall_procstat(interrupt_frame_t* frame) {
    uint32_t     pid = (uint32_t)frame->ebx;
    proc_stat_t* buf = (proc_stat_t*)frame->ecx;
    if (!buf) return SYSCALL_ERROR;

    process_t* p = (pid == 0) ? get_current_process() : scheduler_find_process(pid);
    if (!p) return SYSCALL_ERROR;

    buf->pid        = (int)p->pid;
    buf->parent_pid = (int)p->parent_pid;
    strncpy(buf->name, p->name, PROCESS_NAME_MAX - 1);
    buf->name[PROCESS_NAME_MAX - 1] = '\0';
    strncpy(buf->cwd, p->cwd, PROCESS_CWD_MAX - 1);
    buf->cwd[PROCESS_CWD_MAX - 1] = '\0';

    return 0;
}

// ebx=pid   returns 0 when target exits
static int syscall_wait(interrupt_frame_t* frame) {
    uint32_t pid = (uint32_t)frame->ebx;
    process_t* cur = get_current_process();

    if (pid == cur->pid)
        printf("wait: WARNING: process %d waiting for itself\n", cur->pid);

    if (!scheduler_find_process(pid)) return 0;  // already gone

    cur->waiting_for_pid = pid;
    cur->blocked = true;
    process_yield();
    // Execution resumes here after the target process exits and unblocks us
    return 0;
}

static int syscall_mkdir(interrupt_frame_t* frame)
{
    char* abs_path = (char*) frame->ebx;
    return fs_create_dir(abs_path) == FS_OK ? SYSCALL_SUCCESS : SYSCALL_ERROR;
}

static syscall_func_t sys_table[SYSCALL_COUNT] = {
    [SYSCALL_EXIT]        = syscall_exit,
    [SYSCALL_WRITE]       = syscall_write,
    [SYSCALL_READ]        = syscall_read,
    [SYSCALL_OPEN]        = syscall_open,
    [SYSCALL_SBRK]        = syscall_sbrk,
    [SYSCALL_CLOSE]       = syscall_close,
    [SYSCALL_FSTAT]       = syscall_fstat,
    [SYSCALL_ISATTY]      = syscall_isatty,
    [SYSCALL_LSEEK]       = syscall_lseek,
    [SYSCALL_KILL]        = syscall_kill,
    [SYSCALL_CREATE_PROC] = syscall_create_proc,
    [SYSCALL_PS]          = syscall_ps,
    [SYSCALL_WAIT]        = syscall_wait,
    [SYSCALL_PROCSTAT]    = syscall_procstat,
    [SYSCALL_MKDIR]       = syscall_mkdir
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