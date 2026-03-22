#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>

#define SYSCALL_EXIT    0
#define SYSCALL_WRITE   1
#define SYSCALL_READ    2
#define SYSCALL_SBRK    3
#define SYSCALL_CLOSE   4
#define SYSCALL_FSTAT   5
#define SYSCALL_ISATTY  6
#define SYSCALL_LSEEK   7
#define SYSCALL_OPEN    8

static inline int do_syscall(uintptr_t num, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    int ret;
    asm volatile (
        "int $0x67"
        : "=a"(ret)                          
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

// newlib imports these without underscores alias them for convenience 
int write(int file, char *ptr, int len) __attribute__((alias("_write")));
void* sbrk(int incr)                    __attribute__((alias("_sbrk")));
void exit(int status)                   __attribute__((alias("_exit")));
int close(int file)                     __attribute__((alias("_close")));
int fstat(int file, struct stat *st)    __attribute__((alias("_fstat")));
int isatty(int file)                     __attribute__((alias("_isatty")));
int lseek(int file, int ptr, int dir)   __attribute__((alias("_lseek")));
int open(const char *path, int flags, int mode) __attribute__((alias("_open")));
int read(int file, char *ptr, int len)  __attribute__((alias("_read")));


void _exit(int status) {
    do_syscall(SYSCALL_EXIT, (uintptr_t)status, 0, 0);
    while(1);
}

int _write(int file, char *ptr, int len) {
    return do_syscall(SYSCALL_WRITE, (uintptr_t)file, (uintptr_t)ptr, (uintptr_t)len);
}

int _read(int file, char *ptr, int len) {
    return do_syscall(SYSCALL_READ, (uintptr_t)file, (uintptr_t)ptr, (uintptr_t)len);
}

void *_sbrk(int incr) {
    return (void *)do_syscall(SYSCALL_SBRK, (uintptr_t)incr, 0, 0);
}

int _close(int file) {
    return do_syscall(SYSCALL_CLOSE, (uintptr_t)file, 0, 0);
}

int _fstat(int file, struct stat *st) {
    return do_syscall(SYSCALL_FSTAT, (uintptr_t)file, (uintptr_t)st, 0);
}

int _isatty(int file) {
    return do_syscall(SYSCALL_ISATTY, (uintptr_t)file, 0, 0);
}

int _lseek(int file, int ptr, int dir) {
    return do_syscall(SYSCALL_LSEEK, (uintptr_t)file, (uintptr_t)ptr, (uintptr_t)dir);
}

int _open(const char *path, int flags, int mode) {
    return do_syscall(SYSCALL_OPEN, (uintptr_t)path, (uintptr_t)flags, (uintptr_t)mode);
}

int kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
int getpid(void) { return 1; }
