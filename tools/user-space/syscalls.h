#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stddef.h>
#include <stdint.h>

#define SYSCALL_EXIT        0
#define SYSCALL_WRITE       1
#define SYSCALL_READ        2
#define SYSCALL_SBRK        3
#define SYSCALL_CLOSE       4
#define SYSCALL_FSTAT       5
#define SYSCALL_ISATTY      6
#define SYSCALL_LSEEK       7
#define SYSCALL_OPEN        8
#define SYSCALL_KILL        9
#define SYSCALL_CREATE_PROC 10
#define SYSCALL_PS          11
#define SYSCALL_WAIT        12
#define SYSCALL_PROCSTAT    13
#define SYSCALL_MKDIR       14
#define SYSCALL_SLEEP       15
#define SYSCALL_WAKE        16
#define SYSCALL_YIELD       17

// Args struct for create_proc_by_elf syscall
typedef struct {
    const char*  path;
    int          argc;
    const char** argv;
    const char*  cwd;
} proc_create_args_t;

// Process stats struct — returned by procstat()
typedef struct {
    int  pid;
    int  parent_pid;
    char name[64];
    char cwd[256];
} proc_stat_t;

int  create_proc_by_elf(const proc_create_args_t* args);
int  ps(uint32_t* buf, int max_entries);
int  wait_pid(int pid);
int create_dir(char* abs_path);
// pid=0 means current process
int  procstat(int pid, proc_stat_t* out);
int yield(void);
int _sleep(uint32_t ms);
int wake(int pid);

#endif /* SYSCALLS_H */