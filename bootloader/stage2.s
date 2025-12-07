.code16
    .org 0x0000

/*
* bootloader/stage2.s
* Second stage bootloader - loads kernel and switches to protected mode
*/

# Multiboot constants
.equ MULTIBOOT_MAGIC, 0x2BADB002
.equ MULTIBOOT_FLAGS, 0x00000003  # ALIGN | MEMINFO
.equ MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

# Memory layout
.equ KERNEL_LOAD_ADDR, 0x200000    # 2MB - where kernel will be loaded (32-bit accessible)
.equ KERNEL_TEMP_BUFFER, 0x10000   # Temporary buffer for loading in 16-bit mode
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
    
    # Print stage 2 message
    movw $stage2_msg, %si
    call print_string
    
    # Step 1: Enable A20 line
    call enable_a20
    movw $a20_msg, %si
    call print_string
    
    # Step 2: Load kernel to temporary buffer (16-bit accessible)
    call load_kernel_temp
    movw $kernel_temp_loaded_msg, %si
    call print_string
    
    # Step 3: Set up GDT and switch to protected mode
    call setup_gdt
    call enter_protected_mode
    
    # This should never be reached
    jmp hang

# Enable A20 line using keyboard controller method
enable_a20:
    pusha
    
    
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

# Load kernel from disk to temporary buffer (16-bit accessible)
load_kernel_temp:
    pusha
    
    # Set up segment for 16-bit addressing
    # ES:0x10000 = 0x1000:0x0000 (segment 0x1000, offset 0x0000)
    movw $0x1000, %ax
    movw %ax, %es
    
    # Load kernel to temporary buffer at 0x10000
    movw $0x0000, %bx     # Offset 0 in segment 0x1000 = 0x10000
    movb $KERNEL_START_SECTOR, %cl
    movb $KERNEL_SECTORS, %al
    
    # Read kernel sectors
    movb boot_drive, %dl
    call read_sectors
    
    # Restore ES segment
    xor %ax, %ax
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
    movl $0x30000, %esp
    
    # Step 1: Copy kernel from temporary buffer to final location
    call copy_kernel_to_final
    # Step 2: Search for multiboot header and verify
    call find_multiboot_header_32
    testl %eax, %eax
    jz multiboot_error_32
    # Step 3: Build multiboot info structure (in 32-bit mode)
    call build_multiboot_info_32
    
    # Step 4: Set up registers for kernel
    movl $MULTIBOOT_MAGIC, %eax    # Magic number
    movl $multiboot_info_32, %ebx  # Multiboot info structure
    # Jump to kernel entry point at 0x200000
    # The linker sets ENTRY(_start) which should be at KERNEL_LOAD_ADDR
    movl $KERNEL_LOAD_ADDR + 0xc, %edx
    jmp *%edx

# Copy kernel from temporary buffer to final location (0x200000)
copy_kernel_to_final:
    pusha
    
    # Calculate size: KERNEL_SECTORS * 512 bytes
    movl $KERNEL_SECTORS, %ecx
    shll $9, %ecx                  # Multiply by 512 (2^9)
    
    # Source: temporary buffer at 0x10000
    movl $KERNEL_TEMP_BUFFER, %esi
    # Destination: final location at 0x200000
    movl $KERNEL_LOAD_ADDR, %edi
    
    # Copy using rep movsb
    cld
    rep movsb
    
    popa
    ret

# Find multiboot header in loaded kernel (32-bit mode)
find_multiboot_header_32:
    pusha
    
    # Start searching from kernel load address (0x200000)
    movl $KERNEL_LOAD_ADDR, %eax
    movl %eax, %edi
    movl $0x2000, %ecx       # Search first 8KB (multiboot requirement)
    
.search_loop:
    # Check if we have a valid multiboot header
    # Read dword from memory first (safer for debugging) then compare
    movl (%edi), %eax
    cmpl $0x1BADB002, %eax
    je .found_header
    
.next_dword:
    addl $4, %edi
    subl $4, %ecx
    jnz .search_loop
    
    # Header not found
    xorl %eax, %eax
    jmp .done
    
.found_header:
    movl $1, %eax
    
.done:
    # Store result
    movl %eax, result_32
    
    popa
    movl result_32, %eax      # Get result
    ret

# Build multiboot info structure in 32-bit mode
build_multiboot_info_32:
    pusha
    
    # Set flags = 1 (memory info available)
    movl $multiboot_info_32, %edi
    movl $0x1, (%edi)
    addl $4, %edi
    
    # mem_lower and mem_upper - simplified values
    # These would ideally be queried from BIOS, but we're in 32-bit mode now
    movl $640, (%edi)              # mem_lower (640KB)
    addl $4, %edi
    movl $0x100000, (%edi)         # mem_upper (1MB, simplified)
    
    popa
    ret

# Error handler in 32-bit mode
multiboot_error_32:
    movl $multiboot_error_msg_32, %esi
    call print_string_32
hang_32:
    cli
    hlt
    jmp hang_32

# Print string in 32-bit mode (simplified - uses VGA text mode directly)
# ESI should contain the string address
print_string_32:
    pusha
    movl $0xB8000, %edi            # VGA text buffer
    movb $0x0F, %al                # White on black
    
    # Get current cursor position (simplified - just append)
    # For now, we'll use a simple approach and append to screen
    # In a real implementation, you'd track cursor position
    movl vga_cursor_pos, %edi
    addl $0xB8000, %edi
    
.print_loop_32:
    lodsb                          # Load byte from string
    testb %al, %al
    jz .print_done_32
    cmpb $'\r', %al
    je .print_loop_32              # Skip carriage return
    cmpb $'\n', %al
    je .handle_newline
    stosw                          # Store char + attribute
    jmp .print_loop_32
    
.handle_newline:
    # Move to next line (80 chars per line, 2 bytes per char)
    movl vga_cursor_pos, %edi
    addl $160, %edi                # Next line (80 * 2)
    movl %edi, vga_cursor_pos
    addl $0xB8000, %edi
    jmp .print_loop_32
    
.print_done_32:
    movl %edi, %eax
    subl $0xB8000, %eax
    movl %eax, vga_cursor_pos
    popa
    ret

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
result_32: .long 0
vga_cursor_pos: .long 0

# Messages
stage2_msg: .asciz "Stage 2: Starting bootloader...\r\n"
a20_msg: .asciz "A20 line enabled.\r\n"
kernel_temp_loaded_msg: .asciz "Kernel loaded to temporary buffer.\r\n"
disk_error_msg: .asciz "Disk read error!\r\n"
multiboot_error_msg: .asciz "Multiboot header not found!\r\n"
switch_to_protected_mode_msg: .asciz "Switching to protected mode...\r\n"
multiboot_error_msg_32: .asciz "Multiboot header not found!\r\n"

# GDT
.align 4
gdt:
    .quad 0x0000000000000000    # Null descriptor
    .quad 0x00CF9A000000FFFF    # Code descriptor (32-bit, 4GB)
    .quad 0x00CF92000000FFFF    # Data descriptor (32-bit, 4GB)

gdt_descriptor:
    .word gdt_descriptor - gdt - 1
    .long gdt

# Multiboot info structure (16-bit mode - not used anymore)
.align 4
multiboot_info:
    .long 0    # flags
    .long 0    # mem_lower
    .long 0    # mem_upper

# Multiboot info structure (32-bit mode)
.align 4
multiboot_info_32:
    .long 0    # flags
    .long 0    # mem_lower
    .long 0    # mem_upper
