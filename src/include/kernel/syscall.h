#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <arch/i686/idt.h>
#include <stdint.h>

// Syscall numbers
#define SYSCALL_EXIT    0
#define SYSCALL_WRITE   1
#define SYSCALL_READ    2

#define SYSCALL_COUNT   3

void syscall_init();

#endif