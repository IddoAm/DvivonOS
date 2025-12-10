.code16
    .org 0x0000

/*
* bootloader/stage2.s
* Second stage bootloader - loads kernel, gets memory map, switches to PM
*/

# Multiboot constants
.equ MULTIBOOT_MAGIC, 0x2BADB002
.equ MULTIBOOT_FLAGS, 0x00000003  # ALIGN | MEMINFO
.equ MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

# Memory layout
.equ KERNEL_LOAD_ADDR, 0x200000    # 2MB - where kernel will be loaded
.equ KERNEL_TEMP_BUFFER, 0x10000   # Temporary buffer for loading
.equ MMAP_BUFFER, 0x3000           # Buffer for E820 Memory Map
.equ KERNEL_START_SECTOR, 4        
.equ KERNEL_SECTORS, 32           

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
    
    # Step 2: Load kernel to temporary buffer
    call load_kernel_temp
    movw $kernel_temp_loaded_msg, %si
    call print_string

    # --- NEW STEP: Get Memory Map (E820) ---
    # Must be done in Real Mode before GDT setup
    call do_e820
    movw $mmap_msg, %si
    call print_string
    # ---------------------------------------
    
    # Step 3: Set up GDT and switch to protected mode
    call setup_gdt
    call enter_protected_mode
    
    # This should never be reached
    jmp hang

# ------------------------------------------------------------------
# NEW: Detect Memory Map (0xE820)
# ------------------------------------------------------------------
do_e820:
    pusha

    mov $MMAP_BUFFER, %di
    xor %ebx, %ebx          # EBX must be 0 for first call
    xor %bp, %bp            # Keep entry count (optional debugging)
    
    mov $0x534D4150, %edx   # Magic 'SMAP'
    mov $0xE820, %eax       # Function Code

.e820_loop:
    # 1. Write the Multiboot "Size" field (20 bytes)
    # Multiboot specs say this field contains the size of the structure 
    # *excluding* the size field itself.
    movl $20, (%di)
    add $4, %di             # Move DI past the size field
    
    # 2. Call BIOS to fill the next 20 bytes
    mov $20, %ecx           # Request 20 bytes
    mov $0xE820, %eax       # Reset function code
    int $0x15
    
    jc .e820_done           # Carry set = error or end of list
    cmpl $0x534D4150, %eax  # Check for 'SMAP' signature
    jne .e820_done

    # 3. Prepare for next entry
    # We wrote 4 bytes (size) + 20 bytes (BIOS data) = 24 bytes total
    add $20, %di
    
    # 4. Check if finished
    test %ebx, %ebx         # If EBX is 0, list is complete
    jz .e820_done
    
    jmp .e820_loop

.e820_done:
    # Calculate total length (End Address - Start Address)
    mov %di, %ax
    sub $MMAP_BUFFER, %ax   # AX = Total bytes written
    movw %ax, mmap_total_length  # Store for 32-bit code to read later
    
    popa
    ret

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
# struct multiboot_info {
#     uint32_t flags;          offset 0
#     uint32_t mem_lower;      offset 4
#     uint32_t mem_upper;      offset 8
#     uint32_t boot_device;    offset 12
#     uint32_t cmdline;        offset 16
#     uint32_t mods_count;     offset 20
#     uint32_t mods_addr;      offset 24
#     uint32_t syms[4];        offset 28-44
#     uint32_t mmap_length;    offset 44
#     uint32_t mmap_addr;      offset 48
# }
build_multiboot_info_32:
    pusha
    
    movl $multiboot_info_32, %edi
    
    # flags: Set bit 6 (0x40) for MMAP, and usually bit 0/1 for basic mem
    # 0x40 (MMAP) | 0x01 (MEM_LOWER/UPPER) = 0x41
    movl $0x41, (%edi)
    addl $4, %edi
    
    # mem_lower (640KB in KB)
    movl $640, (%edi)
    addl $4, %edi
    
    # mem_upper (1024 * 31 = 31MB approx, simplified)
    movl $31744, (%edi)
    addl $4, %edi
    
    # boot_device
    movl $0x8000FFFF, (%edi)
    addl $4, %edi
    
    # cmdline, mods_count, mods_addr, syms[4] -> All Zero
    movl $0, (%edi)
    addl $4, %edi
    movl $0, (%edi)
    addl $4, %edi
    movl $0, (%edi)
    addl $4, %edi
    movl $0, (%edi)
    addl $4, %edi
    movl $0, (%edi)
    addl $4, %edi
    movl $0, (%edi)
    addl $4, %edi
    movl $0, (%edi)
    addl $4, %edi
    
    # --- UPDATED SECTION ---
    # mmap_length
    movw mmap_total_length, %ax  # Read the length calculated in Real Mode
    movl %eax, (%edi)             # Store at offset 44
    addl $4, %edi
    
    # mmap_addr
    movl $MMAP_BUFFER, %eax      # 0x3000

    movl %eax, (%edi)            # Store at offset 48
    
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
mmap_total_length: .word 0     # NEW: Stores the total size of the E820 map

# Messages
stage2_msg: .asciz "Stage 2: Starting bootloader...\r\n"
a20_msg: .asciz "A20 line enabled.\r\n"
kernel_temp_loaded_msg: .asciz "Kernel loaded to temporary buffer.\r\n"
mmap_msg: .asciz "Memory map detected.\r\n"
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

# Multiboot info structure (32-bit mode)
.align 4
multiboot_info_32:
    .long 0    # flags
    .long 0    # mem_lower
    .long 0    # mem_upper
    .long 0    # boot_device
    .long 0    # cmdline
    .long 0    # mods_count
    .long 0    # mods_addr
    .long 0    # syms[0]
    .long 0    # syms[1]
    .long 0    # syms[2]
    .long 0    # syms[3]
    .long 0    # mmap_length
    .long 0    # mmap_addr
