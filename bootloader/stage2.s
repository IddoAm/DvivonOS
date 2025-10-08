.code16
    .org 0x0000

/*
* bootloader/stage2.s
* Second stage bootloader - loads kernel and switches to protected mode
*/

# Multiboot constants
.equ MULTIBOOT_MAGIC, 0x1BADB002
.equ MULTIBOOT_FLAGS, 0x00000003  # ALIGN | MEMINFO
.equ MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

# Memory layout
.equ STAGE2_LOAD_ADDR, 0x7E0       # Stage 2 load address (16-bit accessible)
.equ KERNEL_LOAD_ADDR, 0x10000     # 64KB - where kernel will be loaded (16-bit accessible)
.equ KERNEL_START_SECTOR, 4        # Kernel starts at sector 4 (after stage1 and stage2)
.equ KERNEL_SECTORS, 32           # Maximum sectors to read for kernel
                                # NOTE: This must match KERNEL_MAX_SECTORS in CMakeLists.txt

# GDT constants
.equ GDT_NULL, 0x00
.equ GDT_CODE, 0x08
.equ GDT_DATA, 0x10

_start2:
    cli
    movb %dl, boot_drive
    
    # Initialize segments
    movw $STAGE2_LOAD_ADDR, %ax
    movw %ax, %ds
    movw %ax, %es

    
    # Print stage 2 message
    movw $stage2_msg, %si
    call print_string
    
    # Step 1: Enable A20 line
    call enable_a20
    movw $a20_msg, %si
    call print_string
    
    # Step 2: Load kernel directly to 0x00100000
    call load_kernel
    movw $kernel_loaded_msg, %si
    call print_string
    
    # Step 3: Search for multiboot header and verify
    call find_multiboot_header
    test %ax, %ax
    jz multiboot_error
    movw $multiboot_found_msg, %si
    call print_string
    
    # Step 4: Build multiboot info structure
    call build_multiboot_info
    
    # Step 5: Set up GDT and switch to protected mode
    call setup_gdt
    call enter_protected_mode
    
    # This should never be reached
    jmp hang

# Enable A20 line using keyboard controller method
enable_a20:
    pusha
    
    # Disable interrupts
    cli
    
    # Wait for input buffer to be empty
    call wait_8042_input
    
    # Send command to disable keyboard
    movb $0xAD, %al
    outb %al, $0x64
    
    # Wait for input buffer to be empty
    call wait_8042_input
    
    # Read output port
    movb $0xD0, %al
    outb %al, $0x64
    
    # Wait for output buffer to be full
    call wait_8042_output
    
    # Read the output port value
    inb $0x60, %al
    movb %al, %bl
    
    # Wait for input buffer to be empty
    call wait_8042_input
    
    # Send command to write output port
    movb $0xD1, %al
    outb %al, $0x64
    
    # Wait for input buffer to be empty
    call wait_8042_input
    
    # Write output port with A20 bit set
    movb %bl, %al
    orb $0x02, %al
    outb %al, $0x60
    
    # Wait for input buffer to be empty
    call wait_8042_input
    
    # Re-enable keyboard
    movb $0xAE, %al
    outb %al, $0x64
    
    # Wait for input buffer to be empty
    call wait_8042_input
    
    # Re-enable interrupts
    sti
    
    popa
    ret

wait_8042_input:
    pusha
    movw $0xFFFF, %cx
.wait_input:
    inb $0x64, %al
    testb $0x02, %al
    loopnz .wait_input
    popa
    ret

wait_8042_output:
    pusha
    movw $0xFFFF, %cx
.wait_output:
    inb $0x64, %al
    testb $0x01, %al
    loopz .wait_output
    popa
    ret

# Load kernel from disk directly to 0x10000
load_kernel:
    pusha
    
    # Set up segment for 16-bit addressing
    # We need to set ES to access memory at 64KB
    # ES:0x10000 = 0x1000:0x0000 (segment 0x1000, offset 0x0000)
    movw $0x1000, %ax
    movw %ax, %es
    
    # Load kernel directly to 0x10000
    movw $0x0000, %bx     # Offset 0 in segment 0x1000 = 0x10000
    movb $KERNEL_START_SECTOR, %cl
    movb $KERNEL_SECTORS, %al
    
    # Read kernel sectors
    movb boot_drive, %dl
    call read_sectors
    
    # Restore ES segment
    movw $STAGE2_LOAD_ADDR, %ax
    movw %ax, %es
    
    popa
    ret

# This function is no longer needed - we load directly to 0x100000

# Read sectors from disk
# AL = number of sectors, CL = starting sector, BX = buffer address, DL = drive
read_sectors:
    pusha
    
    movb %al, sectors_to_read
    movb %cl, current_sector
    
.read_loop:
    # Set up disk read
    movb $0x02, %ah          # Read sectors
    movb $0x01, %al          # Read 1 sector at a time
    movb current_sector, %cl # Starting sector
    movb $0x00, %ch          # Cylinder 0
    movb $0x00, %dh          # Head 0
    movb boot_drive, %dl     # Drive
    
    # Perform read
    int $0x13
    jc disk_error
    
    # Move to next sector
    addw $0x200, %bx         # Move buffer pointer 512 bytes
    incb current_sector      # Next sector
    decb sectors_to_read     # Decrement counter
    jnz .read_loop
    
    popa
    ret

# Find multiboot header in loaded kernel
find_multiboot_header:
    pusha
    
    # Set up segment to access kernel at 0x10000
    # ES:0x10000 = 0x1000:0x0000 (segment 0x1000, offset 0x0000)
    movw $0x1000, %ax
    movw %ax, %es
    
    # Start searching from kernel load address (0x10000)
    movw $0x0000, %di   # Offset 0 in segment 0x1000 = 0x10000
    mov $0x2000, %cx       # Search first 8KB (multiboot requirement)
    
.search_loop:
    # Check if we have a valid multiboot header (32-bit comparison in 16-bit mode)
    # Compare lower 16 bits first
    cmp $0xB002, %es:(%di)        # Lower 16 bits of 0x1BADB002
    jne .next_dword
    
    # Compare upper 16 bits
    cmp $0x1BAD, %es:2(%di)       # Upper 16 bits of 0x1BADB002
    je .found_header
    
.next_dword:
    add $4, %di
    sub $4, %cx
    jnz .search_loop
    
    # Header not found
    xor %ax, %ax
    jmp .done
    
.found_header:
    mov $1, %ax
    
.done:
    # Store result in a variable for return
    mov %ax, result
    # Restore ES segment
    movw $STAGE2_LOAD_ADDR, %ax
    movw %ax, %es
    
    popa
    mov result, %ax      # Get result
    ret

# Build minimal multiboot info structure
build_multiboot_info:
    pusha
    
    # Get memory size using BIOS int 0x15, function 0xE820
    mov $multiboot_info, %di
    mov $0x1, (%di)  # flags = 1 (memory info available)
    add $4, %di
    
    # Get lower memory (below 1MB)
    int $0x12                 # Get memory size in KB
    mov %ax, (%di)         # mem_lower
    add $4, %di
    
    # Get upper memory (above 1MB) - simplified
    mov $0x100000, (%di)    # mem_upper (1MB, simplified)
    
    popa
    ret

# Set up Global Descriptor Table
setup_gdt:
    pusha
    
    # Load GDT
    lgdt gdt_descriptor
    
    popa
    ret

# Enter protected mode
enter_protected_mode:
    pusha
    movw $switch_to_protected_mode_msg, %si
    call print_string
    
    # Enable protected mode
    mov %cr0, %eax
    or $0x01, %eax
    mov %eax, %cr0
    
    # Far jump to 32-bit code
    ljmp $GDT_CODE, $protected_mode_start
    
    # This should never be reached
    popa
    ret

# 32-bit protected mode code
.code32
protected_mode_start:
    # Set up data segments
    movw $GDT_DATA, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    
    # Set up stack
    movl $0x90000, %esp
    
    # Set up registers for kernel
    movl $MULTIBOOT_MAGIC, %eax    # Magic number
    movl $multiboot_info, %ebx     # Multiboot info structure
    movw $jmp_kernel_msg, %si
    call print_string
    # Jump to kernel at 0x10000 (where we loaded it)
    mov $0x10000, %edx
    jmp *%edx

# Back to 16-bit code
.code16

# Print string function
print_string:
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

# Error handlers
disk_error:
    movw $disk_error_msg, %si
    call print_string
    jmp hang

multiboot_error:
    movw $multiboot_error_msg, %si
    call print_string
    jmp hang

hang:
    cli
    hlt
    jmp hang

# Data section
boot_drive: .byte 0
sectors_to_read: .byte 0
current_sector: .byte 0
result: .word 0

# Messages
stage2_msg: .asciz "Stage 2: Starting bootloader...\r\n"
a20_msg: .asciz "A20 line enabled.\r\n"
kernel_loaded_msg: .asciz "Kernel loaded to 0x10000.\r\n"
multiboot_found_msg: .asciz "Multiboot header found and verified.\r\n"
disk_error_msg: .asciz "Disk read error!\r\n"
multiboot_error_msg: .asciz "Multiboot header not found!\r\n"
switch_to_protected_mode_msg: .asciz "Switching to protected mode...\r\n"
jmp_kernel_msg: .asciz "Jumping to kernel...\r\n"

# GDT
.align 4
gdt:
    .quad 0x0000000000000000    # Null descriptor
    .quad 0x00CF9A000000FFFF    # Code descriptor (32-bit, 4GB)
    .quad 0x00CF92000000FFFF    # Data descriptor (32-bit, 4GB)

gdt_descriptor:
    .word gdt_descriptor - gdt - 1
    .long gdt

# Multiboot info structure
.align 4
multiboot_info:
    .long 0    # flags
    .long 0    # mem_lower
    .long 0    # mem_upper
