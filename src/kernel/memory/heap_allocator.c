#include<kernel/heap_allocator.h>

void add_free_block(free_block_header_t* block){
    block->next_free = free_list_head;
    if(free_list_head){
        free_list_head->prev_free = block;
    }
    block->prev_free = NULL;
    free_list_head = block;
}

void remove_free_block(free_block_header_t* block){
    if(block->prev_free){
        block->prev_free->next_free = block->next_free;
    }else{
        free_list_head = block->next_free;
    }

    if(block->next_free){
        block->next_free->prev_free = block->prev_free;
    }
}

uintptr_t allocate_new_heap_page(){
    uintptr_t page = kernel_vmm_alloc_page();

    if(!page){
       return 0; // Allocation failed
    }

    heap_pages++;

    free_block_header_t* new_block = (free_block_header_t*)page;
    SET_SIZE(new_block, PAGE_SIZE);
    SET_FREE(new_block);

    add_free_block(new_block);
    
    return page;
}   

uintptr_t kmalloc(uint32_t size){
    // Align size to BLOCK_ALIGN
    size = ALIGN_UP(size);
    // Include header size
    size += sizeof(block_header_t);

    if(size < sizeof(free_block_header_t)){
        size = sizeof(free_block_header_t);
    }

    if(free_list_head == NULL){
        if(!allocate_new_heap_page()){
            return 0; // Allocation failed
        }
    }

    free_block_header_t* current = free_list_head;
    while(current){
        uint32_t current_size = GET_SIZE(current);
        if(current_size >= size) {
            // Found a suitable block
            remove_free_block(current);
            CLEAR_FREE(current);

            if(current_size > size + sizeof(free_block_header_t)){
                // Split the block
                free_block_header_t* new_block = (free_block_header_t*)((uintptr_t)current + size);
                SET_SIZE(new_block, current_size - size);
                SET_FREE(new_block);
                add_free_block(new_block);
                

                SET_SIZE(current, size);
            }

            return (uintptr_t)current + sizeof(block_header_t);
        }
        current = current->next_free;
    }

    // No suitable block found, allocate a new page
    if(!allocate_new_heap_page()){
        return 0; // Allocation failed
    }

    return kmalloc(size - sizeof(block_header_t)); // Retry allocation
}

void kfree(uintptr_t ptr){
    if(ptr == 0) return;

    block_header_t* header = (block_header_t*)(ptr - sizeof(block_header_t));
    free_block_header_t* block = (free_block_header_t*)header;

    SET_FREE(block);
    add_free_block(block);

    // Coalescing with next block
    uintptr_t next_addr = (uintptr_t)block + GET_SIZE(block);
    if(next_addr < MEMORY_SPACE){
        block_header_t* next_header = (block_header_t*)next_addr;
        if(IS_FREE(next_header)){
            free_block_header_t* next_block = (free_block_header_t*)next_header;
            remove_free_block(next_block);
            SET_SIZE(block, GET_SIZE(block) + GET_SIZE(next_block));
        }
    }

    // Coalescing with previous block
    free_block_header_t* current = free_list_head;
    while(current){
        uintptr_t current_end = (uintptr_t)current + GET_SIZE(current);
        if(current_end == (uintptr_t)block){
            // Found previous block
            remove_free_block(current);
            SET_SIZE(current, GET_SIZE(current) + GET_SIZE(block));
            add_free_block(current);
            break;
        }
        current = current->next_free;
    }
}