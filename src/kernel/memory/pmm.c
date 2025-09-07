#include <kernel/pmm.h>
#include <lib/stdio.h>
#include <kernel/memory_defs.h>

static inline void clear_bit(uint32_t bit, uint32_t* bitmap) {
    bitmap[bit / 32] &= ~(1u << (bit % 32));
}

static inline void set_bit(uint32_t bit, uint32_t* bitmap) {
    bitmap[bit / 32] |= (1u << (bit % 32));
}

static inline int test_bit(uint32_t bit, uint32_t* bitmap) {
    return (bitmap[bit / 32] >> (bit % 32)) & 1u;
}

// in words
static uintptr_t usable_end;

void pmm_init(const multiboot_mmap_entry_t* mmap, uint32_t length) {
    multiboot_mmap_entry_t* entry = (multiboot_mmap_entry_t*)mmap;
    uintptr_t mmap_end = (uintptr_t)mmap + length;

    // Set all pages as used initially
    for(uint32_t i = 0; i < MAX_PAGES / 32; i++){
        _pmm_bitmap_start[i] = 0xFFFFFFFF;
    }

    // Mark free pages in bitmap
    while ((uintptr_t)entry < mmap_end) {
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t start = (entry->addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t end   = (entry->addr + entry->len) & ~(PAGE_SIZE - 1);
            
            if(usable_end < end){
                usable_end = end;
            }

            for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
                if (addr < 0x100000000ULL) { // only map first 4GiB in 32-bit mode
                    clear_bit((uint32_t)(addr / PAGE_SIZE), _pmm_bitmap_start);
                }
            }
        }
        entry = (multiboot_mmap_entry_t*)((uintptr_t)entry + entry->size + sizeof(entry->size));
    }

    usable_end = (usable_end / PAGE_SIZE / BITMAP_ENTRY_BITS);

    // Reserve kernel memory
    for (uintptr_t addr = (uintptr_t)_kernel_start; addr < (uintptr_t)_kernel_end; addr += PAGE_SIZE) {
        set_bit(addr / PAGE_SIZE, _pmm_bitmap_start);
    }

    printf("%d\n", usable_end);
    /*
    for (uint32_t j = 0; j < _pmm_bitmap_start_length; j++) {
        if(_pmm_bitmap_start[j] == 0xFFFFFFFF){
            printf("1");
        }else if(_pmm_bitmap_start[j] == 0){
            printf("0");
        }else{
            printf("x");
        } 
    }
    */
}

uintptr_t pmm_alloc_page(void) {
    for (uint32_t i = 0; i < usable_end; i++) { 
        if (!test_bit(i, _pmm_bitmap_start)) {  // free page
            set_bit(i, _pmm_bitmap_start);      // mark used
            return (uintptr_t)i * PAGE_SIZE;
        }
    }
    return 0; // out of memory
}

void pmm_free_page(const uintptr_t addr) {
    uintptr_t page_index = (uintptr_t)addr / PAGE_SIZE;
    clear_bit((uint32_t)page_index, _pmm_bitmap_start); // mark free
}

void adjust_bitmap_address_for_paging(){
   
}