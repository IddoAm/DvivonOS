#include <kernel/heap_allocator.h>

/* ---- Helpers ---- */
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

static inline void add_free_block(heap_context_t* heap, free_block_header_t* block) {
    block->next_free = heap->free_list_head;
    if (heap->free_list_head)
        heap->free_list_head->prev_free = block;
    block->prev_free = NULL;
    heap->free_list_head = block;
}

static inline void remove_free_block(heap_context_t* heap, free_block_header_t* block) {
    if (block->prev_free)
        block->prev_free->next_free = block->next_free;
    else
        heap->free_list_head = block->next_free;

    if (block->next_free)
        block->next_free->prev_free = block->prev_free;

    block->next_free = block->prev_free = NULL;
}

static inline void update_footer(block_header_t* header) {
    block_header_t* footer = get_footer(header);
    footer->size_and_flags = header->size_and_flags;
}

static inline void coalesce_blocks(heap_context_t* heap, free_block_header_t* a, free_block_header_t* b) {
    remove_free_block(heap, b);
    SET_SIZE(a, GET_SIZE(a) + GET_SIZE(b));
    SET_FREE(a);
    update_footer((block_header_t*)a);
}

static inline void coalesce_with_next(heap_context_t* heap, free_block_header_t* block) {
    uintptr_t next_addr = (uintptr_t)block + GET_SIZE(block);
    if (next_addr >= heap->heap_end)
        return;

    block_header_t* next_header = (block_header_t*)next_addr;
    if (IS_FREE(next_header)) {
        coalesce_blocks(heap, block, (free_block_header_t*)next_header);
    }
}

static inline void coalesce_with_prev(heap_context_t* heap, free_block_header_t* block) {
    if ((uintptr_t)block <= heap->heap_start)
        return;

    block_header_t* prev_footer = (block_header_t*)((uintptr_t)block - sizeof(block_header_t));
    if ((uintptr_t)prev_footer < heap->heap_start || !IS_FREE(prev_footer))
        return;

    free_block_header_t* prev_block =
        (free_block_header_t*)((uintptr_t)block - GET_SIZE(prev_footer));
    coalesce_blocks(heap, prev_block, block);
}

static free_block_header_t* split_block(heap_context_t* heap, free_block_header_t* block, uint32_t want_size) {
    uint32_t current_size = GET_SIZE(block);
    if (current_size < want_size + sizeof(free_block_header_t))
        return NULL;

    uintptr_t new_addr = (uintptr_t)block + want_size;
    uint32_t new_size = current_size - want_size;

    SET_SIZE(block, want_size);
    update_footer((block_header_t*)block);

    free_block_header_t* new_block = create_free_block(new_addr, new_size);
    add_free_block(heap, new_block);

    return new_block;
}

/* ---- Page allocation ---- */
static uintptr_t allocate_new_heap_page(heap_context_t* heap) {
    uintptr_t page = vmm_alloc_kernel_page();
    if (!page)
        return 0;

    /* Update heap boundaries */
    if (heap->heap_start == 0) {
        heap->heap_start = page;
        heap->heap_end = page + PAGE_SIZE;
    } else {
        if (page < heap->heap_start)
            heap->heap_start = page;
        if (page + PAGE_SIZE > heap->heap_end)
            heap->heap_end = page + PAGE_SIZE;
    }

    heap->heap_pages++;
    free_block_header_t* block = create_free_block(page, PAGE_SIZE);
    add_free_block(heap, block);
    coalesce_with_prev(heap, block);
    return page;
}

/* ---- Heap operations ---- */
void* heap_alloc(heap_context_t* heap, uint32_t size) {
    if (size == 0)
        return NULL;

    size = ALIGN_UP(size) + sizeof(block_header_t);
    if (size < sizeof(free_block_header_t))
        size = sizeof(free_block_header_t);

    if (!heap->free_list_head) {
        if (!allocate_new_heap_page(heap))
            return NULL;
    }

    free_block_header_t* cur = heap->free_list_head;
    while (cur) {
        uint32_t current_size = GET_SIZE(cur);
        if (current_size >= size) {
            remove_free_block(heap, cur);
            CLEAR_FREE(cur);

            if (current_size >= size + sizeof(free_block_header_t)) {
                split_block(heap, cur, size);
            } else {
                update_footer((block_header_t*)cur);
            }

            return (void*)((uintptr_t)cur + sizeof(block_header_t));
        }
        cur = cur->next_free;
    }

    if (!allocate_new_heap_page(heap))
        return NULL;

    return heap_alloc(heap, size - sizeof(block_header_t));
}

void heap_free(heap_context_t* heap, void* ptr) {
    if (!ptr)
        return;

    free_block_header_t* block = (free_block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    SET_FREE(block);
    update_footer((block_header_t*)block);

    add_free_block(heap, block);
    coalesce_with_prev(heap, block);
    coalesce_with_next(heap, block);
}

/* ---- Kernel heap wrapper ---- */
static heap_context_t kernel_heap;

void heap_init_kernel(uintptr_t start, uint32_t initial_pages) {
    kernel_heap.heap_start = start;
    kernel_heap.heap_end = start;
    kernel_heap.heap_pages = 0;
    kernel_heap.max_pages = 0; // unlimited
    kernel_heap.page_dir = vmm_get_kernel_pd(); // kernel PD

    for (uint32_t i = 0; i < initial_pages; i++)
        allocate_new_heap_page(&kernel_heap);
}

void* kmalloc(uint32_t size) {
    return heap_alloc(&kernel_heap, size);
}

void kfree(void* ptr) {
    heap_free(&kernel_heap, ptr);
}

/* ---- Heap context creation ---- */
heap_context_t* heap_create(uintptr_t start, uint32_t initial_pages, uint32_t max_pages, page_directory_t* pd) {
    heap_context_t* heap = (heap_context_t*)kmalloc(sizeof(heap_context_t));
    if (!heap)
        return NULL;

    heap->heap_start = start;
    heap->heap_end = start;
    heap->heap_pages = 0;
    heap->max_pages = max_pages;
    heap->page_dir = pd;
    heap->free_list_head = NULL;

    for (uint32_t i = 0; i < initial_pages; i++)
        allocate_new_heap_page(heap);

    return heap;
}
