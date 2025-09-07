#ifndef PMM_H
#define PMM_H

#include <boot/multiboot.h>

extern uint32_t* _pmm_bitmap_start;

#define BITMAP_ENTRY_BITS 32

void pmm_init(const multiboot_mmap_entry_t* mmap, const uint32_t length);

uintptr_t pmm_alloc_page();
void pmm_free_page(const uintptr_t addr);

void adjust_bitmap_address_for_paging();

#endif