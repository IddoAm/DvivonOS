
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define INTURRUPT_COUNT 256
#define MAX_HANDELERS_PER_INTURRUPT 4


#define IDT_TRAP_GATE_USER      0xEF  // Present, DPL=3, 32-bit trap gate

#define SYSCALL_INT 0x67

// IDT configuration constants
#define IDT_ALIGNMENT 0x10
#define IDT_LIMIT_OFFSET 1
#define EXCEPTION_COUNT 32
#define IRQ_COUNT 16
#define IRQ_SLAVE_THRESHOLD 40 // EXCEPTION_COUNT + 8

// GDT selector constants
#define KERNEL_CS_SELECTOR 0x08

// IDT gate flags
#define IDT_INTERRUPT_GATE 0x8E // Present, Ring 0, 32-bit interrupt gate

// Bit manipulation constants
#define MASK_LOW_16BIT 0xFFFF
#define SHIFT_HIGH_16BIT 16

// PIC remap offsets
#define PIC_MASTER_OFFSET 0x20
#define PIC_SLAVE_OFFSET 0x28

// IRQ to vector conversion
#define IRQ_BASE_VECTOR 0x20

extern uint32_t isr_table_start[];
extern uint32_t isr_table_end[];

typedef struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} interrupt_frame_t;

static inline int irq_to_vector(int irq) {
    return irq + IRQ_BASE_VECTOR;
}

static inline int vector_to_irq(int vec) {
    return vec - IRQ_BASE_VECTOR;
}

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);

typedef void (*interrupt_handler_t)(interrupt_frame_t* frame);

void isr_register_handler(uint8_t num, interrupt_handler_t handler);
void isr_unregister_handler(uint8_t num, interrupt_handler_t handler);

#endif