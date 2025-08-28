#include <kernel/pmm.h>
#include <lib/stdio.h>

static inline void clear_bit(uint32_t bit, uint32_t* bitmap) {
    bitmap[bit / 32] &= ~(1u << (bit % 32));
}

static inline void set_bit(uint32_t bit, uint32_t* bitmap) {
    bitmap[bit / 32] |= (1u << (bit % 32));
}

static inline uintptr_t align_up(uintptr_t value, uintptr_t  alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static inline uintptr_t align_down(uintptr_t value, uintptr_t  alignment) {
    return value & ~(alignment - 1);
}

static uint32_t* pmm_bitmap;
static uint32_t pmm_bitmap_length;

void pmm_init(const multiboot_mmap_entry_t* mmap, uint32_t length) {
    multiboot_mmap_entry_t* entry = (multiboot_mmap_entry_t*)mmap;
    uintptr_t mmap_end = (uintptr_t)mmap + length;

    // Find max address safely
    uint64_t max_addr = 0;
    while ((uintptr_t)entry < mmap_end) {
        uint64_t region_end = entry->addr + entry->len;
        if (region_end > max_addr) {
            max_addr = region_end;
        }
        entry = (multiboot_mmap_entry_t*)((uintptr_t)entry + entry->size + sizeof(entry->size));
    }

    uint64_t total_pages = (max_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    pmm_bitmap_length = (total_pages + BITMAP_ENTRY_BITS - 1) / BITMAP_ENTRY_BITS;

    // Place bitmap after kernel
    uintptr_t kernel_end = align_up((uintptr_t)_kernel_end, PAGE_SIZE);
    pmm_bitmap = (uint32_t*)kernel_end;

    // Set all as unusable
    for (uint32_t i = 0; i < pmm_bitmap_length; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }

    // Mark free regions
    entry = (multiboot_mmap_entry_t*)mmap;
    while ((uintptr_t)entry < mmap_end) {
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t start = (entry->addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t end   = (entry->addr + entry->len) & ~(PAGE_SIZE - 1);

            for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
                if (addr < 0x100000000ULL) { // only map first 4GiB in 32-bit mode
                    clear_bit((uint32_t)(addr / PAGE_SIZE), pmm_bitmap);
                }
            }
        }
        entry = (multiboot_mmap_entry_t*)((uintptr_t)entry + entry->size + sizeof(entry->size));
    }

    // Reserve kernel + bitmap pages
    uintptr_t kernel_start = align_down((uintptr_t)_kernel_start, PAGE_SIZE);
    size_t bitmap_bytes = pmm_bitmap_length * sizeof(uint32_t);
    uintptr_t bitmap_start = kernel_end;
    uintptr_t bitmap_end   = align_up(bitmap_start + bitmap_bytes, PAGE_SIZE);

    for (uintptr_t addr = kernel_start; addr < bitmap_end; addr += PAGE_SIZE) {
        set_bit(addr / PAGE_SIZE, pmm_bitmap);
    }

    
    printf("%d\n", pmm_bitmap_length);
    /*
    for (uint32_t j = 0; j < pmm_bitmap_length; j++) {
        if(pmm_bitmap[j] == 0xFFFFFFFF){
            printf("1");
        }else if(pmm_bitmap[j] == 0){
            printf("0");
        }else{
            printf("x");
        } 
    }
    */
}

