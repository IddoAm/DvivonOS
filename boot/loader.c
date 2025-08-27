
#include <stdint.h>
#include <stddef.h>

#include <boot/loader.h>

static multiboot_mmap_entry_t* mmap;
static uint32_t mmap_length;

void loader_init(uint32_t magic, uint32_t addr) {
    if (magic != MULTIBOOT_MAGIC) {
        for(;;);
    }

    multiboot_info_t* mb_info = (multiboot_info_t*) addr;

    if (mb_info->flags & MULTIBOOT_INFO_MEM_MAP) {
        mmap = (multiboot_mmap_entry_t*) mb_info->mmap_addr;
        mmap_length = mb_info->mmap_length;

    }
}

const multiboot_mmap_entry_t* loader_get_memory_map(void) {
    return mmap;
}

const uint32_t loader_get_memory_map_length(void) {
    return mmap_length;
}