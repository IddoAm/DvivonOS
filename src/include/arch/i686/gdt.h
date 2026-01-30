#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* GDT configuration constants */
/* increase to include: null, kcode, kdata, ucode, udata, TSS (reserve) */
#define GDT_ENTRY_COUNT 6
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
/* user descriptors: same as kernel but DPL=3 (add 0x60) */
#define GDT_ACCESS_USER_CODE   0xFA /* Present, Ring 3, Code, Executable, Readable */
#define GDT_ACCESS_USER_DATA   0xF2 /* Present, Ring 3, Data, Writable */

/* convenience selector values (index<<3 with RPL for user selectors) */
#define GDT_KERNEL_CODE_SEL 0x08
#define GDT_KERNEL_DATA_SEL 0x10
#define GDT_USER_CODE_SEL   0x1B /* index 3 (0x18) | RPL 3 */
#define GDT_USER_DATA_SEL   0x23 /* index 4 (0x20) | RPL 3 */
/* reserve a selector for the TSS at index 5: 0x28 */
#define GDT_TSS_SEL         0x28

/* GDT granularity flags */
#define GDT_GRANULARITY_FLAGS 0xC /* Granularity=1 (4KB pages), 32-bit=1 */

void gdt_init(void);
void tss_set_stack(uint32_t esp0);

#endif