#ifndef VMM_H
#define VMM_H

#include <stdint.h>

/* ---- Page flags ---- */
#define PAGE_PRESENT     0x001
#define PAGE_RW          0x002
#define PAGE_USER        0x004
#define PAGE_WRITE_THR   0x008
#define PAGE_CACHE_DIS   0x010
#define PAGE_ACCESSED    0x020
#define PAGE_DIRTY       0x040
#define PAGE_4MB         0x080
#define PAGE_GLOBAL      0x100

/* ---- Index masks/constants ---- */
#define PD_INDEX_SHIFT     22
#define PT_INDEX_SHIFT     12
#define PAGE_OFFSET_MASK   0xFFF
#define PAGE_FRAME_MASK    0xFFFFF000
#define PD_PT_INDEX_MASK   0x3FF
#define PAGE_FLAGS_MASK    0xFFF

#define VMM_FREE_LIST_MAX_SIZE 128

#define PD_WINDOW 0xFFFFF000
#define PTS_WINDOW 0xFFC00000

#define PD_SCRATCH_WINDOW (PD_WINDOW - PAGE_SIZE)
#define PTS_SCRATCH_WINDOW (PTS_WINDOW - PAGE_SIZE)

typedef uint32_t page_table_entry_t;

/* ---- Free list ---- */
typedef struct free_list_node {
    struct free_list_node* next;
    struct free_list_node* prev;
} free_list_node_t;

typedef struct free_list {
    free_list_node_t* head;
    free_list_node_t* tail;
    uint32_t count;
} free_list_t;

/* Provided by linker */
extern uint32_t _page_directory_start[];
extern uint32_t _page_tables_start[];

/* Initialize kernel page directory object */
void vmm_init(void);

/* Context switching */
void     vmm_write_cr3(uint32_t phys_addr);
uint32_t vmm_read_cr3(void);
void     vmm_flush_cr3(void);
void     vmm_switch_address_space(uint32_t phys_addr);

/* Generic allocator APIs */
uintptr_t vmm_alloc_kernel_page(void);
void vmm_free_kernel_page(uintptr_t addr);

/* Create user address space (kernel high half copied) */
uint32_t vmm_create_address_space(void);
void vmm_destroy_address_space(uint32_t pd_phys);

#endif
