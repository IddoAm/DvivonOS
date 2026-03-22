#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <arch/i686/idt.h>
#include <stdint.h>

// Syscall numbers
#define SYSCALL_EXIT    0
#define SYSCALL_WRITE   1
#define SYSCALL_READ    2
#define SYSCALL_SBRK    3
#define SYSCALL_CLOSE   4
#define SYSCALL_FSTAT   5
#define SYSCALL_ISATTY  6
#define SYSCALL_LSEEK   7
#define SYSCALL_OPEN    8

#define SYSCALL_COUNT   9

#define SYSCALL_SUCCESS  0
#define SYSCALL_ERROR   -1

void syscall_init();

#endif