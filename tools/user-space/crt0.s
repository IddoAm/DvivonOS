# crt0.s - The very first code executed in userspace

msg:
    .ascii "[HD] cr0\n"
    .set MSG_LEN, . - msg

.section .text
.globl _start

_start:
    # 1. Set up the base pointer for debugging/stack traces
    xorl %ebp, %ebp
    pushl %ebp
    movl %esp, %ebp

    call _write
    # 2. Call the C main function
    # Note: For a more advanced OS, you'd push argc/argv here
    call main

    # 3. Use the return value of main as the exit status
    pushl %eax
    call _write
    call _exit

    # 4. If _exit fails to terminate, hang the CPU
.halt:
    hlt
    jmp .halt

_write:
    movl $1, %eax        # Syscall number
    movl $1, %ebx        # Arg 1
    movl $msg, %ecx      # Arg 2: Pointer to message
    movl $MSG_LEN, %edx  # Arg 3: Length
    int $0x67            # Your custom syscall interrupt