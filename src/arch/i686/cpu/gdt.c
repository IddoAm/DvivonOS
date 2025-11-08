// gdt.h
#include <stdint.h>
#include <kernel/memory_defs.h>

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;     // bits 0..15 of limit
    uint16_t base_low;      // bits 0..15 of base
    uint8_t  base_mid;      // bits 16..23 of base
    uint8_t  access;        // access flags
    uint8_t  gran;          // high 4 bits of limit, then flags
    uint8_t  base_high;     // bits 24..31 of base
};

struct __attribute__((packed)) gdt_ptr {
    uint16_t limit;
    uint32_t base;          // (linear) address of first gdt_entry
};

// Three entries: null, code, data
static struct gdt_entry gdt[3];
static struct gdt_ptr   gp;

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags_high_nibble)
{
    gdt[i].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt[i].base_low  = (uint16_t)(base & 0xFFFF);
    gdt[i].base_mid  = (uint8_t)((base >> 16) & 0xFF);
    gdt[i].access    = access;
    gdt[i].gran      = (uint8_t)(((limit >> 16) & 0x0F) | (flags_high_nibble << 4));
    gdt[i].base_high = (uint8_t)((base >> 24) & 0xFF);
}

// Extern assembly stub
extern void gdt_load(struct gdt_ptr* gp);

void gdt_init(void)
{
    // pointer tells CPU size-1 and address
    gp.limit = sizeof(gdt) - 1;
    //gp.base  = (uint32_t)&gdt[0];
    gp.base = (uint32_t)((uintptr_t)&gdt[0] + KERNEL_HIGHER_HALF);

    // 0) null
    gdt_set_entry(0, 0, 0, 0, 0);

    // 1) kernel code: base=0, limit=0xFFFFF, access=0x9A, flags=0xC (G=1, DB=1)
    gdt_set_entry(1, 0x00000000, 0x000FFFFF, 0x9A, 0xC);

    // 2) kernel data: base=0, limit=0xFFFFF, access=0x92, flags=0xC
    gdt_set_entry(2, 0x00000000, 0x000FFFFF, 0x92, 0xC);

    // Load it & reload segment registers
    gdt_load(&gp);
}
