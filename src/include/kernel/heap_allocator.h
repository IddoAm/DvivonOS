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

/* Size/flag helpers */
#define GET_SIZE(b)         ((b)->size_and_flags & ~BLOCK_FREE)
#define SET_SIZE(b, sz)     ((b)->size_and_flags = ((b)->size_and_flags & BLOCK_FREE) | (sz))
#define IS_FREE(b)          ((b)->size_and_flags & BLOCK_FREE)
#define SET_FREE(b)         ((b)->size_and_flags |= BLOCK_FREE)
#define CLEAR_FREE(b)       ((b)->size_and_flags &= ~BLOCK_FREE)

/* ---- Heap Context ---- */
typedef struct heap_context {
    free_block_header_t* free_list_head;
    uintptr_t heap_start;
    uintptr_t heap_end;
    uint32_t heap_pages;
    uint32_t max_pages;
    page_directory_t* page_dir;
} heap_context_t;

/* ---- Kernel Heap API ---- */
void heap_init_kernel(uintptr_t start, uint32_t initial_pages);
void* kmalloc(uint32_t size);
void  kfree(void* ptr);

/* ---- Userspace Heap API ---- */
void heap_init(heap_context_t* heap,
               uintptr_t start,
               uint32_t initial_pages,
               uint32_t max_pages,
               page_directory_t* page_dir);

heap_context_t* heap_create(uintptr_t start,
                            uint32_t initial_pages,
                            uint32_t max_pages,
                            page_directory_t* page_dir);

void* heap_alloc(heap_context_t* heap, uint32_t size);
void  heap_free(heap_context_t* heap, void* ptr);

/* Optional helpers */
uintptr_t heap_get_start(heap_context_t* heap);
uintptr_t heap_get_end(heap_context_t* heap);

#endif // HEAP_ALLOCATOR_H
