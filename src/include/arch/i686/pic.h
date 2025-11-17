#ifndef PIC_H
#define PIC_H

#include "io.h"
#include "ports.h"
#include <stdint.h>

#define PIC_EOI 0x20
#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

static inline void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC_SLAVE_CMD, PIC_EOI);
    outb(PIC_MASTER_CMD, PIC_EOI);
}

static inline void pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t master_mask = inb(PIC_MASTER_DATA);
    uint8_t slave_mask = inb(PIC_SLAVE_DATA);

    // Start initialization sequence (cascade mode)
    outb(PIC_MASTER_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC_SLAVE_CMD, ICW1_INIT | ICW1_ICW4);

    // Set new interrupt vector offsets
    outb(PIC_MASTER_DATA, offset1);
    outb(PIC_SLAVE_DATA, offset2);

    // Setup cascading
    outb(PIC_MASTER_DATA, 0x04); // Master PIC has a slave at IRQ2
    outb(PIC_SLAVE_DATA, 0x02);  // Slave PIC cascade identity

    // Set environment info
    outb(PIC_MASTER_DATA, ICW4_8086);
    outb(PIC_SLAVE_DATA, ICW4_8086);

    // Restore saved masks
    outb(PIC_MASTER_DATA, master_mask);
    outb(PIC_SLAVE_DATA, slave_mask);
}

static inline void pic_set_mask(uint8_t irq_line) {
    uint16_t port;
    uint8_t value;

    if (irq_line < 8)
        port = PIC_MASTER_DATA;
    else {
        port = PIC_SLAVE_DATA;
        irq_line -= 8;
    }

    value = inb(port) | (1 << irq_line);
    outb(port, value);
}

static inline void pic_clear_mask(uint8_t irq_line) {
    uint16_t port;
    uint8_t value;

    if (irq_line < 8)
        port = PIC_MASTER_DATA;
    else {
        port = PIC_SLAVE_DATA;
        irq_line -= 8;
    }

    value = inb(port) & ~(1 << irq_line);
    outb(port, value);
}

static inline void pic_disable_all() {
    outb(PIC_MASTER_DATA, 0xFF);
    outb(PIC_SLAVE_DATA, 0xFF);
}

#endif // PIC_H
