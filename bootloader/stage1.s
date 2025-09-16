.code16

_start_label:
    jmp _start

.equ SECTOR_AMOUNT, 1
.equ SECTOR_START, 2
.equ KERNEL_OFFSET, 0x1000

_start:
    cli
    movb %dl, boot_drive
    xorw %ax, %ax
    movw %ax, %ss
    movw $0x7c00, %sp
    movw $0x07c0, %ax
    movw %ax, %ds
    movw %ax, %es
    movw $msg, %si
    call print
    movw $0x0000, %ax
    movw %ax, %es
    movw $KERNEL_OFFSET, %bx
    movb $SECTOR_AMOUNT, %bl
    movb $SECTOR_START, %cl
    movb $0x00, %ch
    movb $0x00, %dh
    movb boot_drive, %dl

read_loop:
    pusha
    movb $0x02, %ah
    movb $0x01, %al
    int $0x13
    popa
    jc disk_read_error
    addw $0x0200, %bx
    incb %cl
    decb %bl
    jnz read_loop
    movw $loaded_msg, %si
    call print
    sti
    jmp $0x0000, $KERNEL_OFFSET

print:
    pusha
    cld
.print_loop:
    lodsb
    testb %al, %al
    jz .print_done
    movb $0x0e, %ah
    int $0x10
    jmp .print_loop
.print_done:
    popa
    ret

disk_read_error:
    cli
    movw $disk_err_msg, %si
    call print
hang:
    hlt
    jmp hang

boot_drive: .byte 0
msg:        .asciz "Boot stage 1: loading kernel..."
loaded_msg: .asciz "Kernel loaded -- jumping...\r\n"
disk_err_msg:.asciz "Disk read error!"

.fill 510 - (. - _start_label), 1, 0
.word 0xAA55
