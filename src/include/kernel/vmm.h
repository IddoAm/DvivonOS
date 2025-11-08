#ifndef VMM_H
#define VMM_H

#include <stdint.h>

typedef uint32_t page_table_entry_t;

#define PAGE_PRESENT   0x001
#define PAGE_RW        0x002
#define PAGE_USER      0x004
#define PAGE_WRITE_THR 0x008
#define PAGE_CACHE_DIS 0x010
#define PAGE_ACCESSED  0x020
#define PAGE_DIRTY     0x040
#define PAGE_4MB       0x080
#define PAGE_GLOBAL    0x100

static const uint32_t VMM_FREE_LIST_MAX_SIZE = 128;

static inline void set_page_entry(page_table_entry_t *entry, uintptr_t phys_addr, uint32_t flags) {
    *entry = (phys_addr & 0xFFFFF000) | (flags & 0xFFF);
}

static inline uintptr_t get_phys_addr(page_table_entry_t entry) {
    return entry & 0xFFFFF000;
}

static inline uint32_t get_flags(page_table_entry_t entry) {
    return entry & 0xFFF;
}

extern void enable_paging(uint32_t page_directory_phys, uint32_t kernel_entry);
extern uint32_t read_cr3(void);
extern void write_cr3(uint32_t cr3);
extern void reload_cr3(uint32_t cr3);
extern void invlpg(void* addr);

extern uint32_t _page_directory_start[];
extern uint32_t _page_tables_start[];

void vmm_init();

typedef struct free_list_node {
    struct free_list_node* next;
    struct free_list_node* prev;
} free_list_node_t;

typedef struct free_list {
    free_list_node_t* head;
    free_list_node_t* tail;
    uint32_t count;
} free_list_t;


uintptr_t kernel_vmm_alloc_page();
void kernel_vvmm_free_page(const uintptr_t addr);

void vmm_map_kernel_hh(uint32_t vaddr, uint32_t phys_addr, uint32_t flags);
uint32_t vmm_virt_to_phys(uint32_t vaddr);
void vmm_remove_identity_mapping();
uint32_t vmm_create_page_directory();
void vmm_switch_page_directory(uint32_t cr3_phys);
void vmm_flush_tlb();
void vmm_invalidate_page(void* vaddr);
uint32_t vmm_get_current_cr3();


#endif