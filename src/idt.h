
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);

typedef void (*irq_handler_t)();

#define IRQ_COUNT 2 // out of 16 only 2 are handled for now

void irq_register_handler(int irq, irq_handler_t handler);
void irq_unregister_handler(int irq);

#endif