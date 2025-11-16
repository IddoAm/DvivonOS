#ifndef PMM_H
#define PMM_H

#include <boot/multiboot.h>
#include <kernel/memory-defs.h>

extern uint32_t _pmm_bitmap_start[];

/* Bitmap constants */
#define BITMAP_ENTRY_BITS 32
#define BITMAP_ALL_BITS_SET 0xFFFFFFFFU  /* All 32 bits set (used = 1) */
#define BITMAP_WORD_INDEX_SHIFT 5         /* log2(32) for dividing by 32 */
#define BITMAP_BIT_MASK 0x1F              /* 31 = 0x1F, for bit % 32 */

/* Memory address constants */
#define MEMORY_4GB_LIMIT 0x100000000ULL   /* 4GB limit for 32-bit mode */
#define PAGE_ALIGN_MASK (PAGE_SIZE - 1)   /* Mask for page alignment */

void pmm_init(const multiboot_mmap_entry_t* mmap, const uint32_t length);

uintptr_t pmm_alloc_page();
void pmm_free_page(const uintptr_t addr);

void adjust_bitmap_address_for_paging();

#endif