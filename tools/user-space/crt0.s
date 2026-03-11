# crt0.s - The very first code executed in userspace

.section .text
.globl _start

_start:
    # 1. Set up the base pointer for debugging/stack traces
    xorl %ebp, %ebp
    pushl %ebp
    movl %esp, %ebp

    # 2. Call the C main function
    # TODO: implement push argc/argv here
    call main

    # 3. Use the return value of main as the exit status
    pushl %eax
    call _exit

    # 4. If _exit fails to terminate, hang the CPU
.halt:
    hlt
    jmp .halt
