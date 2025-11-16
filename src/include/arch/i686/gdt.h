
#ifndef GDT_H
#define GDT_H

/* GDT configuration constants */
#define GDT_ENTRY_COUNT 3
#define GDT_LIMIT_OFFSET 1

/* Bit manipulation constants */
#define MASK_LOW_16BIT 0xFFFF
#define MASK_LOW_8BIT 0xFF
#define MASK_LOW_4BIT 0x0F
#define SHIFT_MID_BYTE 16
#define SHIFT_HIGH_BYTE 24
#define SHIFT_HIGH_LIMIT 16
#define SHIFT_GRAN_FLAGS 4

/* GDT segment base and limit */
#define GDT_SEGMENT_BASE 0x00000000
#define GDT_SEGMENT_LIMIT 0x000FFFFF /* 20 bits, represents 4GB with granularity */

/* GDT access byte flags */
#define GDT_ACCESS_KERNEL_CODE 0x9A /* Present, Ring 0, Code, Executable, Readable */
#define GDT_ACCESS_KERNEL_DATA 0x92 /* Present, Ring 0, Data, Writable */

/* GDT granularity flags */
#define GDT_GRANULARITY_FLAGS 0xC /* Granularity=1 (4KB pages), 32-bit=1 */

#endif