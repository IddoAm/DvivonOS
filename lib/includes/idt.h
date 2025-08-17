
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);


typedef void (*irq_handler_t)();
#define IRQ_COUNT 16

void irq_register_handler(uint8_t irq, irq_handler_t handler);
void irq_unregister_handler(uint8_t irq);


typedef void (*excption_handler_t)(uint8_t error_code);
#define EXCEPTION_COUNT 32

void exception_register_handler(uint8_t exception, excption_handler_t handler);
void exception_unregister_handler(uint8_t exception);

#endif