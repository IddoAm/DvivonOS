
#ifndef MEMORY_DEFS_H
#define MEMORY_DEFS_H


#include <stdint.h>
#include <stdbool.h>

#define KERNEL_HIGHER_HALF 0xC0000000

#define PAGE_SIZE 4096
#define MAX_PAGES 1048576               // 4GB / 4KB pages
#define PAGE_DIR_TABLE_SIZE 1024      // 4MB per page directory
#define PAGE_TABLE_COUNT 256     // 1GB kernel / 4MB per page table

static bool paging_enabled = false;

static inline uintptr_t align_up(uintptr_t value, uintptr_t  alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static inline uintptr_t align_down(uintptr_t value, uintptr_t  alignment) {
    return value & ~(alignment - 1);
}

extern char _kernel_start[];
extern char _kernel_end[];

#endif
