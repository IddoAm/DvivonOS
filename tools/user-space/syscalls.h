#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stddef.h>

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
int mkdir(char* abs_path);
// pid=0 means current process
int  procstat(int pid, proc_stat_t* out);

// Implemented by other developers:
int  getpid(void);
void yield(void);

#endif /* SYSCALLS_H */
