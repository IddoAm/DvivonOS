#ifndef VMM_H
#define VMM_H

#include <stdint.h>

typedef uint32_t page_table_entry_t;
typedef uintptr_t free_list_node_t;

#define PAGE_PRESENT   0x001
#define PAGE_RW        0x002
#define PAGE_USER      0x004
#define PAGE_WRITE_THR 0x008
#define PAGE_CACHE_DIS 0x010
#define PAGE_ACCESSED  0x020
#define PAGE_DIRTY     0x040
#define PAGE_4MB       0x080
#define PAGE_GLOBAL    0x100

static inline void set_page_entry(page_table_entry_t *entry, uintptr_t phys_addr, uint32_t flags) {
    *entry = (phys_addr & 0xFFFFF000) | (flags & 0xFFF);
}

static inline uintptr_t get_phys_addr(page_table_entry_t entry) {
    return entry & 0xFFFFF000;
}

static inline uint32_t get_flags(page_table_entry_t entry) {
    return entry & 0xFFF;
}

extern uint32_t _page_directory_start[];
extern uint32_t _page_tables_start[];


extern void enable_paging(uint32_t page_directory_phys, uint32_t kernel_entry);


void vmm_init();

uintptr_t kernel_vmm_alloc_page();
void kernel_vvmm_free_page(const uintptr_t addr);


#endif