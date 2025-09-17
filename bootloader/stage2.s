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
.equ STAGE2_LOAD_ADDR, 0x7E00      # Stage 2 load address (16-bit accessible)
.equ KERNEL_LOAD_ADDR, 0x100000    # 1MB - where kernel will be loaded (32-bit address)
.equ KERNEL_TEMP_ADDR, 0x8000      # Temporary load address (16-bit accessible)
.equ KERNEL_START_SECTOR, 3        # Kernel starts at sector 3 (after stage1 and stage2)
.equ KERNEL_SECTORS, 64            # Maximum sectors to read for kernel

# GDT constants
.equ GDT_NULL, 0x00
.equ GDT_CODE, 0x08
.equ GDT_DATA, 0x10

_start2:
    cli
    movb %dl, boot_drive
    
    # Initialize segments
    movw $0, %ax
    movw %ax, %ds
    movw %ax, %es

    
    # Print stage 2 message
    movw $stage2_msg, %si
    call print_string
    
    # Step 1: Enable A20 line
    call enable_a20
    movw $a20_msg, %si
    call print_string
    
    # Step 2: Load kernel to 0x00100000
    call load_kernel
    movw $kernel_loaded_msg, %si
    call print_string
    
    # Step 3: Search for multiboot header and verify
    call find_multiboot_header
    testl %eax, %eax
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

# Load kernel from disk to memory
load_kernel:
    pusha
    
    # Load kernel to temporary location first (below 1MB)
    # We'll move it to 0x100000 after enabling A20
    movw $KERNEL_TEMP_ADDR, %bx     # Load to 0x8000 (32KB mark)
    movb $KERNEL_START_SECTOR, %cl
    movb $KERNEL_SECTORS, %al
    
    # Read kernel sectors
    movb boot_drive, %dl
    call read_sectors
    
    # Now move kernel from 0x8000 to 0x100000
    call move_kernel_to_high_memory
    
    popa
    ret

# Move kernel from 0x8000 to 0x100000
move_kernel_to_high_memory:
    pusha
    
    # Set up source and destination
    movl $KERNEL_TEMP_ADDR, %esi    # Source: 0x8000
    movl $KERNEL_LOAD_ADDR, %edi    # Destination: 0x100000
    movl $0x8000, %ecx             # Move 32KB (0x8000 bytes)
    
    # Copy 4 bytes at a time
.move_loop:
    movl (%esi), %eax
    movl %eax, (%edi)
    addl $4, %esi
    addl $4, %edi
    subl $4, %ecx
    jnz .move_loop
    
    popa
    ret

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
    
    # Start searching from kernel load address
    movl $KERNEL_LOAD_ADDR, %edi
    movl $0x2000, %ecx       # Search first 8KB (multiboot requirement)
    
.search_loop:
    # Check if we have a valid multiboot header
    cmpl $MULTIBOOT_MAGIC, (%edi)
    jne .next_dword
    
    # Found magic number, check flags and checksum
    movl 4(%edi), %eax       # Get flags
    movl 8(%edi), %ebx       # Get checksum
    
    # Verify checksum: magic + flags + checksum should equal 0
    addl $MULTIBOOT_MAGIC, %eax
    addl %ebx, %eax
    testl %eax, %eax
    jz .found_header
    
.next_dword:
    addl $4, %edi
    subl $4, %ecx
    jnz .search_loop
    
    # Header not found
    xorl %eax, %eax
    jmp .done
    
.found_header:
    movl %edi, multiboot_header_addr
    movl $1, %eax
    
.done:
    movl %eax, %esp          # Store result in stack for return
    popa
    movl %esp, %eax          # Get result
    ret

# Build minimal multiboot info structure
build_multiboot_info:
    pusha
    
    # Get memory size using BIOS int 0x15, function 0xE820
    movl $multiboot_info, %edi
    movl $0x00000001, (%edi)  # flags = 1 (memory info available)
    addl $4, %edi
    
    # Get lower memory (below 1MB)
    int $0x12                 # Get memory size in KB
    movl %eax, (%edi)         # mem_lower
    addl $4, %edi
    
    # Get upper memory (above 1MB) - simplified
    movl $0x100000, (%edi)    # mem_upper (1MB, simplified)
    
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
    
    # Enable protected mode
    movl %cr0, %eax
    orl $0x01, %eax
    movl %eax, %cr0
    
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
    
    # Jump to kernel
    movl multiboot_header_addr, %edx
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
multiboot_header_addr: .long 0

# Messages
stage2_msg: .asciz "Stage 2: Starting bootloader...\r\n"
a20_msg: .asciz "A20 line enabled.\r\n"
kernel_loaded_msg: .asciz "Kernel loaded to 0x100000.\r\n"
multiboot_found_msg: .asciz "Multiboot header found and verified.\r\n"
disk_error_msg: .asciz "Disk read error!\r\n"
multiboot_error_msg: .asciz "Multiboot header not found!\r\n"

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
