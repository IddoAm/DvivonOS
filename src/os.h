#ifndef OS_H
#define OS_H

#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}


static inline void outb(uint16_t port, uint8_t data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "dN"(port));
}

static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif 