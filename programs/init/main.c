#include <stdio.h>
#include "syscalls.h"

int main(void) {
    printf("[init] PID 1 starting...\n");

    proc_create_args_t args = {
        .path = "/public-bin/terminal",
        .argc = 0,
        .argv = NULL,
        .cwd  = "/",
    };

    int pid = create_proc_by_elf(&args);
    if (pid < 0) {
        printf("[init] FATAL: could not start terminal\n");
        while (1) {}
    }
    printf("[init] terminal started (PID %d)\n", pid);

    // Keep PID 1 alive so the scheduler always has a process.
    // yield() will be replaced with a real syscall by other developers.
    while (1) {
        yield();
    }

    return 0;
}
