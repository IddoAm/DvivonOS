
#include <stddef.h>
#include <stdint.h>

#include <boot/loader.h>

#include <lib/stdio.h>
static multiboot_mmap_entry_t* mmap;
static uint32_t mmap_length;

void loader_init(uint32_t magic, uint32_t virt_addr, uint32_t phys_addr) {
    if (magic != MULTIBOOT_MAGIC)
        for (;;);

    multiboot_info_t* mb_info = (multiboot_info_t*)virt_addr;

    if (mb_info->flags & MULTIBOOT_INFO_MEM_MAP) {
        // Fix pointer to point inside copied buffer
        mmap = (multiboot_mmap_entry_t*)((uintptr_t)virt_addr + (mb_info->mmap_addr - phys_addr));
        mmap_length = mb_info->mmap_length;
    }
}

const multiboot_mmap_entry_t* loader_get_memory_map(void) {
    return mmap;
}

const uint32_t loader_get_memory_map_length(void) {
    return mmap_length;
}