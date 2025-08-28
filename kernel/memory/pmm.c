#include "pmm.h"

static uint32_t* pmm_bitmap;

void pmm_init(const multiboot_mmap_entry_t* mmap, const uint32_t length){
    multiboot_mmap_entry_t* entry = mmap;
    uint32_t mmap_end = (uint32_t)length + length;

    multiboot_mmap_entry_t* last = 0;

    while ((uintptr_t)mmap < mmap_end) {
        last = mmap;
        mmap = (multiboot_mmap_entry_t*)((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
    }   

    uint64_t max_addr = last->addr + last->len;

    uint32_t page_count = max_addr / PAGE_SIZE;
    uint32_t bitmap_size = max_addr / BITMAP_ENTRY_BITS;

    if (page_count % 32 != 0) {
        bitmap_size++; 
    }

    // Need to find where to put the bitmap
}