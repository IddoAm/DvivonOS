#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

// -----------------------------
// PIC (Programmable Interrupt Controller)
// -----------------------------
#define PIC_MASTER_CMD      0x20
#define PIC_MASTER_DATA     0x21
#define PIC_SLAVE_CMD       0xA0
#define PIC_SLAVE_DATA      0xA1

// -----------------------------
// PIT (Programmable Interval Timer)
// -----------------------------
#define PIT_CHANNEL0        0x40
#define PIT_CHANNEL1        0x41
#define PIT_CHANNEL2        0x42
#define PIT_COMMAND_PORT    0x43
#define PIT_CONTROL_WORD    0x36  // mode 3, square wave, binary

// -----------------------------
// Keyboard (PS/2)
// -----------------------------
#define KEYBOARD_DATA_PORT  0x60
#define KEYBOARD_CMD_PORT   0x64

// -----------------------------
// CMOS / RTC
// -----------------------------
#define CMOS_ADDRESS        0x70
#define CMOS_DATA           0x71

// -----------------------------
// Serial ports (COM)
// -----------------------------
#define COM1_PORT           0x3F8
#define COM2_PORT           0x2F8
#define COM3_PORT           0x3E8
#define COM4_PORT           0x2E8

// -----------------------------
// VGA ports
// -----------------------------
#define VGA_INDEX_PORT      0x3D4
#define VGA_DATA_PORT       0x3D5
#define VGA_COLOR_MEMORY    0xB8000  // text mode base address (memory-mapped)
#define VGA_GRAPHICS_MEM    0xA0000  // graphics mode base address (memory-mapped)
#define VGA_CRTC_INDEX      0x3D4
#define VGA_CRTC_DATA       0x3D5
#define VGA_SEQ_INDEX       0x3C4
#define VGA_SEQ_DATA        0x3C5
#define VGA_GC_INDEX        0x3CE
#define VGA_GC_DATA         0x3CF
#define VGA_AC_INDEX        0x3C0
#define VGA_AC_WRITE        0x3C0
#define VGA_AC_READ         0x3C1
#define VGA_MISC_WRITE      0x3C2
#define VGA_MISC_READ       0x3CC
#define VGA_INPUT_STATUS_1  0x3DA

// -----------------------------
// Floppy controller
// -----------------------------
#define FLOPPY_DOR          0x3F2
#define FLOPPY_MSR          0x3F4
#define FLOPPY_FIFO         0x3F5
#define FLOPPY_CTRL         0x3F7

// -----------------------------
// IDE / ATA
// -----------------------------
#define ATA0_DATA           0x1F0
#define ATA0_ERROR          0x1F1
#define ATA0_SECTOR_COUNT   0x1F2
#define ATA0_LBA_LOW        0x1F3
#define ATA0_LBA_MID        0x1F4
#define ATA0_LBA_HIGH       0x1F5
#define ATA0_DRIVE_HEAD     0x1F6
#define ATA0_COMMAND        0x1F7
#define ATA0_STATUS         0x1F7
#define ATA1_DATA           0x170
#define ATA1_ERROR          0x171
#define ATA1_SECTOR_COUNT   0x172
#define ATA1_LBA_LOW        0x173
#define ATA1_LBA_MID        0x174
#define ATA1_LBA_HIGH       0x175
#define ATA1_DRIVE_HEAD     0x176
#define ATA1_COMMAND        0x177
#define ATA1_STATUS         0x177

// -----------------------------
// Mouse (PS/2) — often shares keyboard controller
// -----------------------------
#define PS2_MOUSE_DATA      0x60
#define PS2_MOUSE_CMD       0x64

#endif // PORTS_H
