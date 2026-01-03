#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <arch/i686/idt.h>
#include <stdint.h>

// Syscall numbers
#define SYSCALL_EXIT    0
#define SYSCALL_WRITE   1
#define SYSCALL_READ    2
#define SYSCALL_OPEN    3
#define SYSCALL_CLOSE   4

#define SYSCALL_COUNT   5

// Error codes
#define ESUCCESS    0
#define EBADF      -1   // Bad file descriptor
#define EFAULT     -2   // Bad address
#define EINVAL     -3   // Invalid argument
#define ENOSYS     -4   // Function not implemented

// Handler called from assembly
void syscall_handler(interrupt_frame_t* frame);

#endif