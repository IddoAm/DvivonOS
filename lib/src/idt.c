
#include <idt.h>
#include <os.h>
#include <vga.h>

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
    _idtr.limit = sizeof(idt) - 1;
    _idtr.base  = (uint32_t)&idt;

    for (int i = 0; i < 256; ++i) {
        idt[i].isr_low = 0; idt[i].kernel_cs = 0; idt[i].reserved = 0; idt[i].attributes = 0; idt[i].isr_high = 0;
    }

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

struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;  
    uint32_t eip, cs, eflags, useresp, ss;
};

static irq_handler_t irq_handlers[IRQ_COUNT] = { 0 };

void irq_common_handler(struct regs* r) {
    uint8_t irq = r->int_no - 0x20;   

    if (irq >= 0 && irq < IRQ_COUNT) {
        if (irq_handlers[irq]) {
            irq_handlers[irq]();
        }
    }

    // Send EOI
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq >= 0 && irq < IRQ_COUNT) {
        irq_handlers[irq] = handler;
    }
}

void irq_unregister_handler(uint8_t irq) {
    if (irq >= 0 && irq < IRQ_COUNT) {
        irq_handlers[irq] = 0;
    }
}

static const char* exception_messages[32] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",

    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",

    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

static excption_handler_t excption_handlers[EXCEPTION_COUNT] = { 0 };

void isr_common_handler(struct regs* r) {
    uint8_t code = r->int_no;

    if(code >= 0 && code <= 31){
        // CPU exception
        if(excption_handlers[code]){
            excption_handlers[code](code);
        }else{
            vga_writestring("UNHANDLED EXCEPTION: ");
            vga_writestring(exception_messages[code]);
            vga_writestring("\nSystem Halted.\n");

            __asm__ volatile ("cli; hlt");
        }
    }else{
        vga_writestring("UNHANDLED INTURRPT\n");
        // TODO: add printing of numbers
    }
}

void exception_register_handler(uint8_t exception, excption_handler_t handler) {
    if (exception >= 0 && exception < EXCEPTION_COUNT) {
        excption_handlers[exception] = handler;
    }
}

void exception_unregister_handler(uint8_t exception) {
    if (exception >= 0 && exception < EXCEPTION_COUNT) {
        excption_handlers[exception] = 0;
    }
}