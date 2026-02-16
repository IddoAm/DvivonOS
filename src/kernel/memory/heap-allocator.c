
#include <kernel/heap-allocator.h>

static free_block_header_t* free_list_head = NULL;
static uint32_t heap_max = KHEAP_START;

static inline block_header_t* get_footer(block_header_t* header) {
    return (block_header_t*)((uintptr_t)header + GET_SIZE(header) - sizeof(block_header_t));
}

static free_block_header_t* create_free_block(uintptr_t addr, uint32_t size) {
    free_block_header_t* block = (free_block_header_t*)addr;
    SET_SIZE(block, size);
    SET_FREE(block);

    block_header_t* footer = get_footer((block_header_t*)block);
    footer->size_and_flags = block->size_and_flags;

    block->next_free = block->prev_free = NULL;
    return block;
}

static inline void add_free_block(free_block_header_t* block) {
    block->next_free = free_list_head;
    if (free_list_head)
        free_list_head->prev_free = block;
    block->prev_free = NULL;
    free_list_head = block;
}

static inline void remove_free_block(free_block_header_t* block) {
    if (block->prev_free)
        block->prev_free->next_free = block->next_free;
    else
        free_list_head = block->next_free;
    if (block->next_free)
        block->next_free->prev_free = block->prev_free;
    block->next_free = block->prev_free = NULL;
}

static inline void update_footer(block_header_t* header) {
    block_header_t* footer = get_footer(header);
    footer->size_and_flags = header->size_and_flags;
}

static inline void coalesce_blocks(free_block_header_t* a, free_block_header_t* b) {
    remove_free_block(b);
    SET_SIZE(a, GET_SIZE(a) + GET_SIZE(b));
    SET_FREE(a);
    update_footer((block_header_t*)a);
}

static inline void coalesce_with_next(free_block_header_t* block) {
    uintptr_t next_addr = (uintptr_t)block + GET_SIZE(block);
    if (next_addr >  heap_max || next_addr >= (uintptr_t)MEMORY_SPACE) return;

    block_header_t* next_header = (block_header_t*)next_addr;
    if (IS_FREE(next_header)) {
        coalesce_blocks(block, (free_block_header_t*)next_header);
    }
}

static inline void coalesce_with_prev(free_block_header_t* block) {
    if ((uintptr_t)block <= heap_start)
        return;

    block_header_t* prev_footer = (block_header_t*)((uintptr_t)block - sizeof(block_header_t));
    if (prev_footer <= KHEAP_START || !IS_FREE(prev_footer)) return;

    free_block_header_t* prev_block =
        (free_block_header_t*)((uintptr_t)block - GET_SIZE(prev_footer));
    coalesce_blocks(prev_block, block);
}

static free_block_header_t* split_block(free_block_header_t* block, uint32_t want_size) {
    uint32_t current_size = GET_SIZE(block);
    if (current_size < want_size + sizeof(free_block_header_t))
        return NULL;

    uintptr_t new_addr = (uintptr_t)block + want_size;
    uint32_t new_size = current_size - want_size;

    SET_SIZE(block, want_size);
    update_footer((block_header_t*)block);

    free_block_header_t* new_block = create_free_block(new_addr, new_size);
    add_free_block(new_block);

    return new_block;
}

static uintptr_t allocate_new_heap_page(void) {
    uint32_t addr = heap_max;
    uintptr_t page = vmm_alloc_kernel_page_at(addr);
    if (!page) return 0;

    heap_max += PAGE_SIZE;
    free_block_header_t* block = create_free_block(addr, PAGE_SIZE);
    add_free_block(block);
    coalesce_with_prev(block);
    return page;
}

uintptr_t kmalloc(uint32_t size) {
    size = ALIGN_UP(size);
    // footer + header
    size += 2 * sizeof(block_header_t);
    if (size < sizeof(free_block_header_t))
        size = sizeof(free_block_header_t);

    if (!free_list_head) {
        if (!allocate_new_heap_page())
            return 0;
    }
    free_block_header_t* cur = free_list_head;
    while (cur) {
        uint32_t current_size = GET_SIZE(cur);
        if (current_size >= size) {
            remove_free_block(cur);
            CLEAR_FREE(cur);

            if (current_size >= size + sizeof(free_block_header_t)) {
                split_block(cur, size);
            } else {
                update_footer((block_header_t*)cur);
            }

            return (uintptr_t)cur + sizeof(block_header_t);
        }
        cur = cur->next_free;
    }

    if (!allocate_new_heap_page())
        return 0;
    return kmalloc(size - sizeof(block_header_t));
}

void kfree(uintptr_t ptr) {
    if (!ptr)
        return;

    free_block_header_t* block = (free_block_header_t*)(ptr - sizeof(block_header_t));
    SET_FREE(block);
    update_footer((block_header_t*)block);

    add_free_block(block);
    coalesce_with_prev(block);
    coalesce_with_next(block);
}