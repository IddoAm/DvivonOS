
#include <arch/i686/idt.h>

#include <arch/i686/ports.h>
#include <arch/i686/io.h>

#include <drivers/vga.h>
#include <lib/stdio.h>
#include <lib/string.h>

void pic_remap(void) {
    unsigned char a1, a2;

    // Save masks
    a1 = inb(PIC_MASTER_DATA);
    a2 = inb(PIC_SLAVE_DATA);

    // Start initialization
    outb(PIC_MASTER_CMD, 0x11);
    outb(PIC_SLAVE_CMD, 0x11);

    // Set vector offsets
    outb(PIC_MASTER_DATA, 0x20); // Master PIC → 0x20–0x27
    outb(PIC_SLAVE_DATA, 0x28); // Slave PIC  → 0x28–0x2F

    // Tell Master about Slave at IRQ2 (0000 0100)
    outb(PIC_MASTER_DATA, 0x04);
    // Tell Slave its cascade identity (0000 0010)
    outb(PIC_SLAVE_DATA, 0x02);

    // Set 8086/88 mode
    outb(PIC_MASTER_DATA, 0x01);
    outb(PIC_SLAVE_DATA, 0x01);

    // Restore saved masks
    outb(PIC_MASTER_DATA, a1);
    outb(PIC_SLAVE_DATA, a2);
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

typedef void (*isr_t)(void);

extern isr_t isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
             isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
             isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
             isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31;

extern isr_t irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
             irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15;

static isr_t* const exceptions[32] = {
    &isr0,&isr1,&isr2,&isr3,&isr4,&isr5,&isr6,&isr7,
    &isr8,&isr9,&isr10,&isr11,&isr12,&isr13,&isr14,&isr15,
    &isr16,&isr17,&isr18,&isr19,&isr20,&isr21,&isr22,&isr23,
    &isr24,&isr25,&isr26,&isr27,&isr28,&isr29,&isr30,&isr31
};

static isr_t* const irqs[16] = {
    &irq0,&irq1,&irq2,&irq3,&irq4,&irq5,&irq6,&irq7,
    &irq8,&irq9,&irq10,&irq11,&irq12,&irq13,&irq14,&irq15
};


void idt_init(void) {
    memset(isr_table_start, 0, INTURRUPT_COUNT * MAX_HANDELERS_PER_INTURRUPT * sizeof(uint32_t));

    _idtr.limit = sizeof(idt) - 1;
    _idtr.base  = (uint32_t)&idt;

    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint32_t)exceptions[i], 0x08, 0x8E);
    }

    pic_remap();

    for (int i = 0; i < 16; i++) {
        idt_set_gate(32 + i, (uint32_t)irqs[i], 0x08, 0x8E);
    }

    idt_flush(&_idtr);
    __asm__ volatile ("sti");
}

void isr_common_handler(interrupt_frame_t* frame) {
    // Check if there are handlers registered for this interrupt

    for(int i=0;i<MAX_HANDELERS_PER_INTURRUPT;i++){
        if((interrupt_handler_t)isr_table_start[frame->int_no * MAX_HANDELERS_PER_INTURRUPT + i]){
            uintptr_t ptr = (uintptr_t)isr_table_start[frame->int_no * MAX_HANDELERS_PER_INTURRUPT + i];
            ((interrupt_handler_t)isr_table_start[frame->int_no * MAX_HANDELERS_PER_INTURRUPT + i])(frame);
        }
    }

    // If the interrupt was from IRQ8 or higher, we need to send an EOI to the slave PIC
    if (frame->int_no >= 40) {
        outb(PIC_SLAVE_CMD, 0x20); // Send EOI to slave PIC
    }
    // Always send an EOI to the master PIC
    outb(PIC_MASTER_CMD, 0x20); // Send EOI to master PIC
}


void isr_register_handler(uint8_t num, interrupt_handler_t handler){
    printf("int %d\n", num);
    isr_table_start[num * MAX_HANDELERS_PER_INTURRUPT] = (uint32_t)handler;
}
void isr_unregister_handler(uint8_t num, interrupt_handler_t handler){
    for(int i=0;i<MAX_HANDELERS_PER_INTURRUPT;i++){
        if((interrupt_handler_t)isr_table_start[num * MAX_HANDELERS_PER_INTURRUPT + i] == handler){
            isr_table_start[num * MAX_HANDELERS_PER_INTURRUPT + i] = 0;
            return;
        }
    }
}