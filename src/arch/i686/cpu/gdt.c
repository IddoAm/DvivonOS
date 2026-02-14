// gdt.h
#include <arch/i686/gdt.h>
#include <stdint.h>
/* kernel string helper already in repo */
#include <lib/string.h>

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

struct __attribute__((packed)) tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
};

// Three entries: null, code, data
static struct gdt_entry gdt[GDT_ENTRY_COUNT];
static struct gdt_ptr gp;

/* file‑scope TSS and kernel interrupt stack */
static struct tss_entry tss;
#define KERNEL_INTERRUPT_STACK_SIZE 0x4000
static uint8_t kernel_interrupt_stack[KERNEL_INTERRUPT_STACK_SIZE];

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
extern void tss_load(uint16_t sel);

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

    // 3) user code segment (flat model, DPL=3)
    gdt_set_entry(3, GDT_SEGMENT_BASE, GDT_SEGMENT_LIMIT, GDT_ACCESS_USER_CODE,
                  GDT_GRANULARITY_FLAGS);

    // 4) user data segment (flat model, DPL=3)
    gdt_set_entry(4, GDT_SEGMENT_BASE, GDT_SEGMENT_LIMIT, GDT_ACCESS_USER_DATA,
                  GDT_GRANULARITY_FLAGS);


    /* initialize the minimal TSS fields we need (tss is file-scope now) */
    memset(&tss, 0, sizeof(tss));
    tss.ss0 = GDT_KERNEL_DATA_SEL;
    tss.esp0 = (uint32_t)(kernel_interrupt_stack + KERNEL_INTERRUPT_STACK_SIZE);
    tss.iomap_base = sizeof(tss); /* disable IO bitmap */

    /* create TSS descriptor at index 5, access 0x89 = present, type=9 (available 32-bit TSS) */
    gdt_set_entry(5, (uint32_t)&tss, (uint32_t)(sizeof(tss) - 1), 0x89, 0x0);

    /* Load GDT and reload segments (assembly stub) */
    gdt_load(&gp);
    /* load TSS selector (assembly helper in lgdt.s) */
    tss_load(GDT_TSS_SEL);
}

void tss_set_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}