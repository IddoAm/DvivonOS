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
static uint32_t usable_end_words;

void pmm_init(const multiboot_mmap_entry_t* mmap, uint32_t length) {
    multiboot_mmap_entry_t* entry = (multiboot_mmap_entry_t*)mmap;
    uintptr_t mmap_end = (uintptr_t)mmap + length;

    // Set all pages as used initially
    for(uint32_t i = 0; i < MAX_PAGES / 32; i++){
        _pmm_bitmap_start[i] = 0xFFFFFFFF;
    }

    uint64_t usable_end = 0;
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

    usable_end_words = (usable_end / PAGE_SIZE / BITMAP_ENTRY_BITS);

    // Reserve kernel memory - convert virtual addresses to physical
    uintptr_t kernel_start_phys = (uintptr_t)_kernel_start - KERNEL_HIGHER_HALF;
    uintptr_t kernel_end_phys = (uintptr_t)_kernel_end - KERNEL_HIGHER_HALF;
    
    for (uint32_t i = 0; i < 256 / 32; i++) {
        _pmm_bitmap_start[i] = 0xFFFFFFFF;
    }
    for (uintptr_t addr = kernel_start_phys; addr < kernel_end_phys; addr += PAGE_SIZE) {
        set_bit(addr / PAGE_SIZE, _pmm_bitmap_start);
    }

    printf("%d\n", usable_end_words);
    printf("bitmap size in words: %d\n", MEMORY_SPACE / PAGE_SIZE / BITMAP_ENTRY_BITS);
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

uint32_t last_alloc = 0;

uintptr_t pmm_alloc_page(void) {
    uint32_t i = last_alloc;

    do {
        uint32_t word = _pmm_bitmap_start[i];
        if (word != 0xFFFFFFFF) {
            int bit = __builtin_ffs(~word) - 1;  // first zero bit
            _pmm_bitmap_start[i] |= (1u << bit);
            // Don't increment last_alloc here - keep it at the same word
            // It will naturally move to the next word when this one fills up
            last_alloc = i;  // Stay at current word
            return ((uintptr_t)i * 32 + bit) * PAGE_SIZE;
        }

        i++;
        if (i >= usable_end_words)
            i = 0;

    } while (i != last_alloc);

    return 0; // no free pages found
}


void pmm_free_page(const uintptr_t addr) {
    uintptr_t page_index = (uintptr_t)addr / PAGE_SIZE;
    clear_bit((uint32_t)page_index, _pmm_bitmap_start); // mark free
}