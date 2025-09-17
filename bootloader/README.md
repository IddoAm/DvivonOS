# Minimal Multiboot Bootloader

This is a minimal bootloader that can load a kernel with a multiboot header and switch to protected mode.

## Features

- **Two-stage bootloader**: Stage 1 loads Stage 2, Stage 2 loads the kernel
- **A20 line enabling**: Uses keyboard controller method to enable access to memory above 1MB
- **Kernel loading**: Loads kernel binary to memory address 0x00100000
- **Multiboot header detection**: Searches for and verifies multiboot header in the first 8KB of kernel
- **Memory information**: Builds minimal multiboot_info structure with memory layout
- **Protected mode**: Sets up GDT and switches to 32-bit protected mode
- **Kernel handoff**: Properly sets up registers and jumps to kernel

## Boot Process

1. **Stage 1 (stage1.s)**:
   - Loaded by BIOS at 0x7C00
   - Sets up basic environment
   - Loads Stage 2 from disk sectors 2-3
   - Jumps to Stage 2

2. **Stage 2 (stage2.s)**:
   - Enables A20 line using keyboard controller
   - Loads kernel from sector 3 onwards to 0x100000
   - Searches for multiboot header in loaded kernel
   - Verifies multiboot header checksum
   - Builds multiboot_info structure with memory information
   - Sets up GDT with code and data segments
   - Switches to protected mode
   - Sets up registers (EAX=magic, EBX=info) and jumps to kernel

## Building

The bootloader is built using CMake. You can build it in two ways:

### Bootloader Only
```bash
./scripts/build_bootloader.sh
```

### Complete System (Bootloader + Kernel)
```bash
./scripts/build_bootloader.sh --complete
```

This will create:
- `build/bootloader/stage1.bin` - First stage bootloader
- `build/bootloader/stage2.bin` - Second stage bootloader  
- `build/bootloader/boot.img` - Bootloader image
- `build/bootloader/complete.img` - Complete system image (with kernel)

## Disk Layout

- Sector 0: Stage 1 bootloader (512 bytes)
- Sector 1: Stage 2 bootloader (512 bytes)
- Sector 2+: Kernel binary

## Memory Layout

- 0x0000-0x7BFF: Available for bootloader use
- 0x7C00-0x7DFF: Stage 1 bootloader (loaded by BIOS)
- 0x7E00-0x7FFF: Stage 2 bootloader (loaded by Stage 1)
- 0x8000-0x8FFF: Bootloader stack and data
- 0x90000-0x9FFFF: Protected mode stack
- 0x100000+: Kernel (loaded by Stage 2)

## Multiboot Compatibility

The bootloader expects a kernel with a multiboot header containing:
- Magic number: 0x1BADB002
- Flags: 0x00000003 (ALIGN | MEMINFO)
- Checksum: -(magic + flags)

The multiboot_info structure provided includes:
- flags: 0x00000001 (memory info available)
- mem_lower: Memory below 1MB in KB
- mem_upper: Memory above 1MB in KB (simplified)

## Testing

To debug the boot loader run those commends in diffent terminals:

* qemu-system-i386 -drive file=build/bootloader/boot.img,format=raw,if=floppy -S -s
* gdb -x scripts/debug_memory.gdb

### note that the boot image need to be there

## Error Handling

The bootloader includes error handling for:
- Disk read errors
- Missing multiboot header
- Invalid multiboot checksum

Error messages are displayed on screen and the system halts.
