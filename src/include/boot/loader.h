#ifndef LOADER_H
#define LOADER_H

#include "multiboot.h"

#define MULTIBOOT_MAGIC 0x2BADB002

void loader_init(uint32_t magic, uint32_t addr);

const multiboot_mmap_entry_t* loader_get_memory_map(void);
const uint32_t loader_get_memory_map_length(void);

#endif