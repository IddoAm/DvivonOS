#ifndef HEAP_ALLOCATOR_H
#define HEAP_ALLOCATOR_H

#include <stdint.h>
#include <kernel/memory_defs.h>
#include <kernel/vmm.h>

#define BLOCK_ALIGN 4
#define ALIGN_UP(x) (((x) + (BLOCK_ALIGN - 1)) & ~(BLOCK_ALIGN - 1))

#define BLOCK_FREE 0x1

typedef struct block_header {
    uint32_t size_and_flags;
} block_header_t;


typedef struct free_block_header {
    uint32_t size_and_flags;
    struct free_block_header* next_free;
    struct free_block_header* prev_free;
} free_block_header_t;

// Macros for size/flag extraction
#define GET_SIZE(b)  ((b)->size_and_flags & ~BLOCK_FREE)
#define SET_SIZE(b, sz) ((b)->size_and_flags = ((b)->size_and_flags & BLOCK_FREE) | (sz))
#define IS_FREE(b)   ((b)->size_and_flags & BLOCK_FREE)
#define SET_FREE(b)   ((b)->size_and_flags |= BLOCK_FREE)
#define CLEAR_FREE(b) ((b)->size_and_flags &= ~BLOCK_FREE)

uintptr_t kmalloc(uint32_t size);
void kfree(uintptr_t ptr);

#endif