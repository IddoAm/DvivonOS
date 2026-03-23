#include <stdio.h>
#include <stdint.h>
#include "syscalls.h"

int main(void) {
    uint32_t entries[64];
    proc_stat_t stat;
    int count = ps(entries, 64);
    if (count < 0) {
        printf("ps: syscall failed\n");
        return 1;
    }
    printf("PID   PPID  NAME\n");
    for (int i = 0; i < count; i++)
        procstat(entries[i], &stat);
        
        printf("%-5d %-5d %s\n",
                stat.pid,
                stat.parent_pid,
                stat.name);
    return 0;
}
