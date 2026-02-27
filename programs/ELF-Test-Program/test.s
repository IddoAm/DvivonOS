.section .data
msg:
    .ascii "Hello from user\n"
    .set MSG_LEN, . - msg

.section .text
.global _start

_start:
    movl $1, %eax        # Syscall number
    movl $1, %ebx        # Arg 1
    movl $msg, %ecx      # Arg 2: Pointer to message
    movl $MSG_LEN, %edx  # Arg 3: Length
    int $0x67            # Your custom syscall interrupt
    
    jmp _start           # Loop forever