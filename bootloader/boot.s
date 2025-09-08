.code16
_start_label:

    jmp _start
    nop

msg:
    .asciz "Hello, World! from bootloader"

_start:
    cli
    xor %ax, %ax
    mov %ax, %ss
    mov $0x7b00, %sp

    mov $0x07C0, %ax   # set DS/ES to 0x7C0
    mov %ax, %ds
    mov %ax, %es

    mov $msg, %si      # point SI to msg
    sti

print:
    cld
    lodsb
    or %al, %al
    jz done
    mov $0x0e, %ah
    int $0x10
    jmp print

done:
    hlt
    jmp done

.fill 510 - (. - _start_label), 1, 0
.word 0xAA55
