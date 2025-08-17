
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

    /* unmask only IRQ0 and IRQ1 on master, mask slave */
    outb(0x21, 0xFC); // 11111100b -> allow IRQ0 and IRQ1 only
    outb(0xA1, 0xFF); // mask all on slave

    unsigned char master = inb(0x21);
    unsigned char slave  = inb(0xA1);

    idt_flush(&_idtr);
    __asm__ volatile ("int $0x21");
    __asm__ volatile ("sti");
}

void irq_common_handler(void* frame) {
    terminal_writestring("la");
    uint32_t* stack = (uint32_t*)frame;
    uint32_t int_no = stack[9];  // offset depends on pusha layout
    terminal_writestring("li");

    int irq = int_no - 32;  // 0 = timer, 1 = keyboard

    if (irq == 0) {
        // timer tick handler
        terminal_writestring("g");
    } else if (irq == 1) {
        uint8_t scancode = inb(0x60);
        // keyboard handler
        terminal_writestring("pressed\n");
    }else{
        terminal_writestring("aaaa\n");
    }

    // Acknowledge PICa
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void isr_common_handler(void* frame) {

}