#ifndef heap-allocator_H
#define heap-allocator_H

#include <kernel/memory-defs.h>
#include <kernel/vmm.h>
#include <stdint.h>

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
#define GET_SIZE(b) ((b)->size_and_flags & ~BLOCK_FREE)
#define SET_SIZE(b, sz) ((b)->size_and_flags = ((b)->size_and_flags & BLOCK_FREE) | (sz))
#define IS_FREE(b) ((b)->size_and_flags & BLOCK_FREE)
#define SET_FREE(b) ((b)->size_and_flags |= BLOCK_FREE)
#define CLEAR_FREE(b) ((b)->size_and_flags &= ~BLOCK_FREE)

static free_block_header_t* free_list_head = NULL;
static uint32_t heap_pages = 0;
static uintptr_t heap_start = 0;
static uintptr_t heap_end = 0;

uintptr_t kmalloc(uint32_t size);
void kfree(uintptr_t ptr);

#endif