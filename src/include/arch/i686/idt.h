
#ifndef IDT_H
#define IDT_H

#include <stdint.h>


#define INTURRUPT_COUNT 256
#define MAX_HANDELERS_PER_INTURRUPT 4

extern uint32_t isr_table_start[];
extern uint32_t isr_table_end[];

typedef struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;  
    uint32_t eip, cs, eflags, useresp, ss;
} interrupt_frame_t;

static inline int irq_to_vector(int irq) {
    return irq + 0x20;
}

static inline int vector_to_irq(int vec) {
    return vec - 0x20;
}

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);


typedef void (*interrupt_handler_t)(interrupt_frame_t* frame);

void isr_register_handler(uint8_t num, interrupt_handler_t handler);
void isr_unregister_handler(uint8_t num, interrupt_handler_t handler);



#endif