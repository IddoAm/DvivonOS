#include "syscalls.h"
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

static inline int do_syscall(uintptr_t num, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    int ret;
    asm volatile("int $0x67" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return ret;
}

// newlib imports these without underscores alias them for convenience
int write(int file, char* ptr, int len) __attribute__((alias("_write")));
void* sbrk(int incr) __attribute__((alias("_sbrk")));
void exit(int status) __attribute__((alias("_exit")));
int close(int file) __attribute__((alias("_close")));
int fstat(int file, struct stat* st) __attribute__((alias("_fstat")));
int isatty(int file) __attribute__((alias("_isatty")));
int lseek(int file, int ptr, int dir) __attribute__((alias("_lseek")));
int open(const char* path, int flags, int mode) __attribute__((alias("_open")));
int read(int file, char* ptr, int len) __attribute__((alias("_read")));

void _exit(int status) {
    do_syscall(SYSCALL_EXIT, (uintptr_t)status, 0, 0);
    while (1)
        ;
}

int _write(int file, char* ptr, int len) {
    return do_syscall(SYSCALL_WRITE, (uintptr_t)file, (uintptr_t)ptr, (uintptr_t)len);
}

int _read(int file, char* ptr, int len) {
    return do_syscall(SYSCALL_READ, (uintptr_t)file, (uintptr_t)ptr, (uintptr_t)len);
}

void* _sbrk(int incr) {
    return (void*)do_syscall(SYSCALL_SBRK, (uintptr_t)incr, 0, 0);
}

int _close(int file) {
    return do_syscall(SYSCALL_CLOSE, (uintptr_t)file, 0, 0);
}

int _fstat(int file, struct stat* st) {
    return do_syscall(SYSCALL_FSTAT, (uintptr_t)file, (uintptr_t)st, 0);
}

int _isatty(int file) {
    return do_syscall(SYSCALL_ISATTY, (uintptr_t)file, 0, 0);
}

int _lseek(int file, int ptr, int dir) {
    return do_syscall(SYSCALL_LSEEK, (uintptr_t)file, (uintptr_t)ptr, (uintptr_t)dir);
}

int _open(const char* path, int flags, int mode) {
    return do_syscall(SYSCALL_OPEN, (uintptr_t)path, (uintptr_t)flags, (uintptr_t)mode);
}

int kill(int pid, int sig) {
    (void)sig; // signal model not implemented; always force-kills
    return do_syscall(SYSCALL_KILL, (uintptr_t)pid, 0, 0);
}

int create_proc_by_elf(const proc_create_args_t* args) {
    return do_syscall(SYSCALL_CREATE_PROC, (uintptr_t)args, 0, 0);
}

int ps(uint32_t* buf, int max_entries) {
    return do_syscall(SYSCALL_PS, (uintptr_t)buf, (uintptr_t)max_entries, 0);
}

int wait_pid(int pid) {
    return do_syscall(SYSCALL_WAIT, (uintptr_t)pid, 0, 0);
}

int create_dir(char* abs_path) {
    return do_syscall(SYSCALL_MKDIR, (uintptr_t)abs_path, 0, 0);
}

int procstat(int pid, proc_stat_t* out) {
    return do_syscall(SYSCALL_PROCSTAT, (uintptr_t)pid, (uintptr_t)out, 0);
}

int yield(void) {
    return do_syscall(SYSCALL_YIELD, 0, 0, 0);
}

int sleep(uint32_t ms) {
    return do_syscall(SYSCALL_SLEEP, (uintptr_t)ms, 0, 0);
}

int wake(int pid) {
    return do_syscall(SYSCALL_WAKE, (uintptr_t)pid, 0, 0);
}
int getpid(void) {
    return 1;
}