#ifndef PMM_H
#define PMM_H

#include <boot/multiboot.h>

#define PAGE_SIZE 4096
#define BITMAP_ENTRY_BITS 32

extern char _kernel_start[];
extern char _kernel_end[];

void pmm_init(const multiboot_mmap_entry_t* mmap, const uint32_t length);

void* pmm_alloc_page();
void pmm_free_page(const void* addr);

#endif