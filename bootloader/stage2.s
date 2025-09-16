.code16
    .org 0x0000

_start2:
    cli
    movw $0x0000, %ax
    movw %ax, %ds
    movw $stage2_msg, %si
    call print2
    sti
    hlt
    jmp .

print2:
    pusha
    cld
.print2_loop:
    lodsb
    testb %al, %al
    jz .print2_done
    movb $0x0e, %ah
    int $0x10
    jmp .print2_loop
.print2_done:
    popa
    ret

stage2_msg:
    .asciz "Hello from Stage2!\r\n"
    