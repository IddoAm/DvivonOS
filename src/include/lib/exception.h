#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdint.h>

// exception struct
typedef struct {
    uint32_t id;
    const char* name;
    const char* message;
} exception_t;

// for now just print to console but it will be used later
void raise_exception(exception_t* exception);
// void exception_init(void);
// void exception_handle_irq(uint32_t irq);
// void exception_handle_trap(uint32_t trap);

#endif // EXCEPTION_H