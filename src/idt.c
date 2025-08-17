
#include "idt.h"
#include "os.h"
#include "vga.h"

void pic_remap(void) {
    unsigned char a1, a2;

    // Save masks
    a1 = inb(0x21);
    a2 = inb(0xA1);

    // Start initialization
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    // Set vector offsets
    outb(0x21, 0x20); // Master PIC → 0x20–0x27
    outb(0xA1, 0x28); // Slave PIC  → 0x28–0x2F

    // Tell Master about Slave at IRQ2 (0000 0100)
    outb(0x21, 0x04);
    // Tell Slave its cascade identity (0000 0010)
    outb(0xA1, 0x02);

    // Set 8086/88 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Restore saved masks
    outb(0x21, a1);
    outb(0xA1, a2);
}

typedef struct {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t     reserved;     // Set to zero
	uint8_t     attributes;   // Type and attributes; see the IDT page
	uint16_t    isr_high;     // The higher 16 bits of the ISR's address
} __attribute__((packed)) idt_entry;

typedef struct {
	uint16_t	limit;
	uint32_t	base;
} __attribute__((packed)) idtr;


// The IDT
__attribute__((aligned(0x10))) 
static idt_entry idt[256];

static idtr _idtr;


void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].isr_low = (uint16_t)(base & 0xFFFF);
    idt[num].kernel_cs     = sel;
    idt[num].reserved = 0;
    idt[num].attributes   = flags;     // e.g. 0x8E: present, ring0, 32-bit int gate
    idt[num].isr_high = (uint16_t)((base >> 16) & 0xFFFF);
}

extern void idt_flush(idtr* idtp);

extern void irq0();
extern void irq1();



void idt_init(void) {
    _idtr.limit = sizeof(idt) - 1;
    _idtr.base  = (uint32_t)&idt;

    for (int i = 0; i < 256; ++i) {
        idt[i].isr_low = 0; idt[i].kernel_cs = 0; idt[i].reserved = 0; idt[i].attributes = 0; idt[i].isr_high = 0;
    }

    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);

    pic_remap();

    unsigned char master = inb(0x21);
    unsigned char slave  = inb(0xA1);

    idt_flush(&_idtr);
    __asm__ volatile ("sti");
}

struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;  
    uint32_t eip, cs, eflags, useresp, ss;
};

static irq_handler_t irq_handlers[IRQ_COUNT] = { 0 };

void irq_common_handler(struct regs* r) {
    int irq = r->int_no - 0x20;   

    if (irq >= 0 && irq < IRQ_COUNT) {
        if (irq_handlers[irq]) {
            irq_handlers[irq]();
        }
    }

    // Send EOI
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void isr_common_handler(struct regs* r) {
    // for CPU exceptions (int_no = 0–31)
}

void irq_register_handler(int irq, irq_handler_t handler) {
    if (irq >= 0 && irq < IRQ_COUNT) {
        irq_handlers[irq] = handler;
    }
}

void irq_unregister_handler(int irq) {
    if (irq >= 0 && irq < IRQ_COUNT) {
        irq_handlers[irq] = 0;
    }
}