#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>

#define SYSCALL_EXIT    0
#define SYSCALL_WRITE   1
#define SYSCALL_READ    2
#define SYSCALL_SBRK    3
#define SYSCALL_CLOSE   4
#define SYSCALL_fSTAT   5
#define SYSCALL_ISATTY  6
#define SYSCALL_LSEEK   7

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

int _write(int file, char *ptr, int len) {
    return do_syscall(SYSCALL_WRITE, (uintptr_t)ptr, (uintptr_t)len, (uintptr_t)file);
}

void * _sbrk(int incr) {
    return (void *)do_syscall(SYSCALL_SBRK, (uintptr_t)incr, 0, 0);
}

void _exit(int status) {
    do_syscall(SYSCALL_EXIT, (uintptr_t)status, 0, 0);
    while(1);
}

int _close(int file) { return -1; }
int _fstat(int file, struct stat *st) { st->st_mode = -1; return 0; }
int _isatty(int file) { return 1; }
int _lseek(int file, int ptr, int dir) { return 0; }
int _read(int file, char *ptr, int len) { return 0; }