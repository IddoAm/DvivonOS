#ifndef IRQ_LOCK_H
#define IRQ_LOCK_H

#include <stdint.h>

typedef struct {
    uint32_t flags;
} interrupt_lock_t;

/**
 * Saves the current EFLAGS and disables interrupts.
 */
static inline void lock_interrupts(interrupt_lock_t* lock) {
    __asm__ volatile(
        "pushfl\n\t"    // Push EFLAGS to stack
        "popl %0\n\t"   // Pop EFLAGS into lock->flags
        "cli"           // Clear Interrupt Flag
        : "=g"(lock->flags)
        :
        : "memory"      // Memory barrier to prevent compiler reordering
    );
}

/**
 * Restores the EFLAGS from a previous state.
 */
static inline void unlock_interrupts(interrupt_lock_t* lock) {
    __asm__ volatile(
        "pushl %0\n\t"  // Push saved flags back to stack
        "popfl"         // Pop into EFLAGS (restores IF bit to original state)
        :
        : "g"(lock->flags)
        : "memory", "cc" // "cc" tells compiler the condition codes might change
    );
}

#endif