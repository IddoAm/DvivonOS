#include <kernel/pmm.h>
#include <lib/stdio.h>

static uint32_t* pmm_bitmap;
static uint32_t pmm_bitmap_length;

static inline void clear_bit(uint32_t bit, uint32_t* bitmap) {
    bitmap[bit / 32] &= ~(1u << (bit % 32));
}

static inline void set_bit(uint32_t bit, uint32_t* bitmap) {
    bitmap[bit / 32] |= (1u << (bit % 32));
}

// Generate a bitmap word covering [bit_start, bit_end) inside one 32-bit word.
// If type == available → bits in range are 0 (free), outside range = 1 (used).
// If type != available → bits in range are 1 (used).
static inline uint32_t make_partial_word(uint32_t bit_start, uint32_t bit_end, uint32_t type) {
    uint32_t mask = 0;

    // build range mask
    uint32_t range = ((1u << (bit_end - bit_start)) - 1u) << bit_start;

    if (type == MULTIBOOT_MEMORY_AVAILABLE) {
        // available → free in range, used outside
        mask = ~range;
    } else {
        // reserved → used everywhere
        mask = 0xFFFFFFFF;
    }

    return mask;
}

static inline uintptr_t align_up(uintptr_t value, uintptr_t  alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static inline uintptr_t align_down(uintptr_t value, uintptr_t  alignment) {
    return value & ~(alignment - 1);
}

void print_hex32(uint32_t val) {
    const char* hex = "0123456789abcdef";
    char buf[9];
    buf[8] = '\0';

    for (int i = 7; i >= 0; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }

    printf("%s", buf);
}

void pmm_init(const multiboot_mmap_entry_t* mmap, const uint32_t length){
    multiboot_mmap_entry_t* entry = mmap;
    uint32_t mmap_end = (uint32_t)mmap + length;

    uintptr_t kernel_end = align_up((uintptr_t)_kernel_end, PAGE_SIZE);
    pmm_bitmap = (uint32_t*)kernel_end;

    uint32_t i = 0;
    uint32_t bit_boundary_index = 0;
    while ((uintptr_t)entry < mmap_end) {
        uintptr_t start = align_up(entry->addr, PAGE_SIZE);
        uintptr_t end = align_down(entry->addr + entry->len, PAGE_SIZE);

        
        uintptr_t end_bitmap_index = align_down(entry->addr + entry->len, PAGE_SIZE) / PAGE_SIZE / BITMAP_ENTRY_BITS;

        pmm_bitmap[i++] = make_partial_word(bit_boundary_index, BITMAP_ENTRY_BITS, entry->type == MULTIBOOT_MEMORY_AVAILABLE ? 0 : 1);

        while(i < end_bitmap_index){
            pmm_bitmap[i] = entry->type == MULTIBOOT_MEMORY_AVAILABLE ? 0 : 0xFFFFFFFF;
            i++;
        }

        bit_boundary_index = (end / PAGE_SIZE) % BITMAP_ENTRY_BITS;
        pmm_bitmap[i] = make_partial_word(0, bit_boundary_index, entry->type == MULTIBOOT_MEMORY_AVAILABLE ? 0 : 1);

        entry = (multiboot_mmap_entry_t*)((uintptr_t)entry + entry->size + sizeof(entry->size));
    }   

    pmm_bitmap_length = i+1;
    uintptr_t kernel_start = align_down((uintptr_t)_kernel_start, PAGE_SIZE);
    size_t bitmap_bytes = pmm_bitmap_length * sizeof(uint32_t);

    uintptr_t bitmap_start = kernel_end;
    uintptr_t bitmap_end   = align_up(bitmap_start + bitmap_bytes, PAGE_SIZE); // page-align for marking

    // Reserve kernel + bitmap pages
    for (uintptr_t addr = kernel_start; addr < bitmap_end; addr += PAGE_SIZE) {
        set_bit(addr / PAGE_SIZE, pmm_bitmap);
    }
    
   // printf("bitmap size %d\n", pmm_bitmap_length);    
    for (uint32_t j = 0; j < pmm_bitmap_length; j++) {
        if(pmm_bitmap[j] == 0xFFFFFFFF){
            printf("1");
        }else{
            printf("%x", pmm_bitmap[j]);
        }    
    }
    
}