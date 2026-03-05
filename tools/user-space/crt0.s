; crt0.s - The very first code executed in userspace
section .text
global _start
extern main
extern _exit

_start:
    ; 1. Set up the base pointer for debugging/stack traces
    xor ebp, ebp
    push ebp
    mov ebp, esp

    ; 2. Call the C main function
    ; Note: For a more advanced OS, you'd push argc/argv here
    call main

    ; 3. Use the return value of main as the exit status
    push eax
    call _exit

    ; 4. If _exit fails to terminate, hang the CPU
.halt:
    hlt
    jmp .halt