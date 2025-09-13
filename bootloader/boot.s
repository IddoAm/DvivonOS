.code16
_start_label:

    jmp _start
    nop

# vars
.equ SECTOR_AMOUNT, 2
.equ KERNEL_OFFSET, 0x1000
BOOT_DRIVE:
    .byte 0
msg:
    .asciz "Hello, World! from bootloader"

#stage1
_start:
    cli
    mov %dl, BOOT_DRIVE  # store boot drive for later use
    # set stack and segments
    xor %ax, %ax
    mov %ax, %ss
    mov $0x7b00, %sp

    mov $0x07C0, %ax   # set DS/ES to 0x7C0
    mov %ax, %ds
    mov %ax, %es

    mov $msg, %si      # point SI to msg
    # call the functions
    call print
    call disk_read
    sti

print:
    pusha
    cld
    lodsb
    or %al, %al
    jz done_print
    mov $0x0e, %ah
    int $0x10
    jmp print
done_print:
    popa
    ret

load_boot:
    pusha
    mov bx, KERNEL_OFFSET
    mov dh, SECTOR_AMOUNT
    mov dl, BOOT_DRIVE
    call disk_read
    popa
    ret

/* Read sectors from disk into memory
 * Inputs:
 *   DL - drive number
 *   DH - number of sectors to read
 *   ES:BX - memory address to read into
 */
disk_read:
    pusha
    push %dx

    mov $0x02, %ah        # BIOS read sectors function
    mov %dh, %al          # number of sectors to read
    mov $0x00, %ch        # cylinder 0
    mov $0x00, %dh        # head 0
    mov $0x02, %cl        # sector 2 (sector 1 is the part 1 bootloader)
    int $0x13              # call BIOS

    jc disk_read_error

    pop %dx
    cmp %al, %dh      # check if all sectors were read
    jne disk_read_error

    popa
    ret

disk_read_error:



#end of stage1
.fill 510 - (. - _start_label), 1, 0
.word 0xAA55

#stage2


