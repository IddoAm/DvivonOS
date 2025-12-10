// gdt.h
#include <arch/i686/gdt.h>
#include <stdint.h>

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low; // bits 0..15 of limit
    uint16_t base_low;  // bits 0..15 of base
    uint8_t base_mid;   // bits 16..23 of base
    uint8_t access;     // access flags
    uint8_t gran;       // high 4 bits of limit, then flags
    uint8_t base_high;  // bits 24..31 of base
};

struct __attribute__((packed)) gdt_ptr {
    uint16_t limit;
    uint32_t base; // (linear) address of first gdt_entry
};

// Three entries: null, code, data
static struct gdt_entry gdt[GDT_ENTRY_COUNT];
static struct gdt_ptr gp;

static void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access,
                          uint8_t flags_high_nibble) {
    gdt[i].limit_low = (uint16_t)(limit & MASK_LOW_16BIT);
    gdt[i].base_low = (uint16_t)(base & MASK_LOW_16BIT);
    gdt[i].base_mid = (uint8_t)((base >> SHIFT_MID_BYTE) & MASK_LOW_8BIT);
    gdt[i].access = access;
    gdt[i].gran = (uint8_t)(((limit >> SHIFT_HIGH_LIMIT) & MASK_LOW_4BIT) |
                            (flags_high_nibble << SHIFT_GRAN_FLAGS));
    gdt[i].base_high = (uint8_t)((base >> SHIFT_HIGH_BYTE) & MASK_LOW_8BIT);
}

// Extern assembly stub
extern void gdt_load(struct gdt_ptr* gp);

void gdt_init(void) {
    // pointer tells CPU size-1 and address
    gp.limit = sizeof(gdt) - GDT_LIMIT_OFFSET;
    gp.base = (uint32_t)&gdt[0];

    // 0) null descriptor (required by x86)
    gdt_set_entry(0, 0, 0, 0, 0);

    // 1) kernel code segment: base=0, limit=4GB, access=GDT_ACCESS_KERNEL_CODE,
    // flags=GDT_GRANULARITY_FLAGS
    gdt_set_entry(1, GDT_SEGMENT_BASE, GDT_SEGMENT_LIMIT, GDT_ACCESS_KERNEL_CODE,
                  GDT_GRANULARITY_FLAGS);

    // 2) kernel data segment: base=0, limit=4GB, access=GDT_ACCESS_KERNEL_DATA,
    // flags=GDT_GRANULARITY_FLAGS
    gdt_set_entry(2, GDT_SEGMENT_BASE, GDT_SEGMENT_LIMIT, GDT_ACCESS_KERNEL_DATA,
                  GDT_GRANULARITY_FLAGS);

    // Load it & reload segment registers
    gdt_load(&gp);
}
