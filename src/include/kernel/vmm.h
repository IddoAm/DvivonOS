#ifndef VMM_H
#define VMM_H

#include <stdint.h>

/* ---- Page flags ---- */
#define PAGE_PRESENT 0x001
#define PAGE_RW 0x002
#define PAGE_USER 0x004
#define PAGE_WRITE_THR 0x008
#define PAGE_CACHE_DIS 0x010
#define PAGE_ACCESSED 0x020
#define PAGE_DIRTY 0x040
#define PAGE_4MB 0x080
#define PAGE_GLOBAL 0x100

/* ---- Page table/directory index constants ---- */
#define PD_INDEX_SHIFT 22          /* Bits to shift for page directory index (bits 22-31) */
#define PT_INDEX_SHIFT 12          /* Bits to shift for page table index (bits 12-21) */
#define PAGE_OFFSET_MASK 0xFFF     /* Mask for page offset (12 bits, 4KB) */
#define PAGE_FRAME_MASK 0xFFFFF000 /* Mask to extract page frame address (clear lower 12 bits) */
#define PD_PT_INDEX_MASK 0x3FF     /* Mask for 10-bit index (1024 entries = 2^10 - 1) */
#define PAGE_FLAGS_MASK 0xFFF      /* Mask for page flags (lower 12 bits) */

#define VMM_FREE_LIST_MAX_SIZE 128

/* ---- Page structures ---- */
typedef uint32_t page_table_entry_t;

typedef struct free_list_node {
    struct free_list_node* next;
    struct free_list_node* prev;
} free_list_node_t;

typedef struct free_list {
    free_list_node_t* head;
    free_list_node_t* tail;
    uint32_t count;
} free_list_t;

/* ---- Linker symbols ---- */
extern uint32_t _page_directory_start[];
extern uint32_t _page_tables_start[];

/* ---- Basic setup ---- */
void vmm_init(void);

/* ---- CR3 / TLB control ---- */
void vmm_write_cr3(uint32_t phys_addr);
uint32_t vmm_read_cr3(void);
void vmm_flush_cr3(void);

/* ---- Page mapping ---- */
void vmm_map_kernel_page(uint32_t vaddr, uint32_t phys_addr, uint32_t flags);
void vmm_unmap_page(uint32_t vaddr);
uint32_t vmm_virt_to_phys(uint32_t vaddr);

/* ---- Kernel virtual memory allocation ---- */
uintptr_t kernel_vmm_alloc_page(void);
void kernel_vmm_free_page(uintptr_t addr);

#endif /* VMM_H */
