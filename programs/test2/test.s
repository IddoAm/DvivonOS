.section .data
msg:
    .ascii "-"
    .set MSG_LEN, . - msg

.section .text
.global _start

_start:
pushl $2
    movl $1, %eax        # Syscall number
    movl $1, %ebx        # Arg 1
    movl $msg, %ecx      # Arg 2: Pointer to message
    movl $MSG_LEN, %edx  # Arg 3: Length
    int $0x67            # Your custom syscall interrupt

    call _exit


_exit:
    movl $0, %eax        # Syscall number for exit
    xorl %ebx, %ebx      # Status code 0
    int $0x67            # Your custom syscall interrupt
