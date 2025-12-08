
#ifndef MEMORY_DEFS_H
#define MEMORY_DEFS_H

#include <lib/stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define KERNEL_HIGHER_HALF 0xC0000000
#define MEMORY_SPACE 0x100000000

#define PAGE_SIZE 4096
#define MAX_PAGES 1048576 // 4GB / 4KB pages
#define PT_ENTRIES 1024
#define PD_ENTRIES 1024
#define PAGE_TABLE_COUNT 256 // 1GB kernel / 4MB per page table
#define PAGE_TABLE_SIZE (PT_ENTRIES * PAGE_SIZE)

static bool paging_enabled = false;

static inline uintptr_t align_up(uintptr_t value, uintptr_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static inline uintptr_t align_down(uintptr_t value, uintptr_t alignment) {
    return value & ~(alignment - 1);
}

extern char _kernel_start[];
extern char _kernel_end[];

#endif
