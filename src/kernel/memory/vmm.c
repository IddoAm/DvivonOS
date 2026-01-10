// kernel/vmm.c
#include <kernel/memory_defs.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <lib/stdio.h>
#include <lib/string.h>

static free_list_t free_list = {0};
static uintptr_t last_free = KERNEL_HIGHER_HALF;

/* ---------------- low-level helpers ---------------- */

uint32_t vmm_read_cr3(void) {
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void vmm_write_cr3(uint32_t phys_addr) {
    __asm__ volatile("mov %0, %%cr3" ::"r"(phys_addr) : "memory");
}

void vmm_flush_cr3(void) {
    uint32_t cr3 = vmm_read_cr3();
    vmm_write_cr3(cr3);
}

static inline void vmm_invlpg(void* addr) {
    __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

static inline uint32_t pd_index(uint32_t v) {
    return (v >> PD_INDEX_SHIFT) & PD_PT_INDEX_MASK;
}
static inline uint32_t pt_index(uint32_t v) {
    return (v >> PT_INDEX_SHIFT) & PD_PT_INDEX_MASK;
}

static void free_list_add(free_list_t* list, free_list_node_t* node) {
    node->next = list->head;
    node->prev = NULL;
    if (list->head)
        list->head->prev = node;
    else
        list->tail = node;
    list->head = node;
    list->count++;
}

static free_list_node_t* free_list_pop(free_list_t* list) {
    if (!list->tail)
        return NULL;
    free_list_node_t* node = list->tail;
    list->tail = node->prev;
    if (list->tail)
        list->tail->next = NULL;
    else
        list->head = NULL;
    node->next = node->prev = NULL;
    list->count--;
    return node;
}

#define PAGE_ALIGN_UP(x) (((uintptr_t)(x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* ---------------- public accessor ---------------- */

page_directory_t* vmm_get_kernel_pd(void) {
    return kernel_pd;
}

/* ---------------- vmm init / switch ---------------- */

void vmm_init(void) {
    if (kernel_pd != NULL)
        return;

    static page_directory_t kernel_pd_storage;
    kernel_pd = &kernel_pd_storage;

    kernel_pd->virt = _page_directory_start;
    kernel_pd->phys = vmm_read_cr3() & PAGE_FRAME_MASK;
    kernel_pd->is_kernel = 1;
}

void vmm_switch_address_space(page_directory_t* pd) {
    if (!pd)
        return;
    vmm_write_cr3(pd->phys);
    vmm_flush_cr3();
}

/* ---------------- address translation / is_mapped ---------------- */

static uint32_t vmm_virt_to_phys(uint32_t vaddr) {
    uint32_t* pt_window = (uint32_t*)0xFFC00000;
    uint32_t* pd_window = (uint32_t*)0xFFFFF000;

    if (!(pd_window[vaddr >> 22] & PAGE_PRESENT)) {
        return 0;
    }

    uint32_t pte = pt_window[vaddr >> 12];
    if (!(pte & PAGE_PRESENT)) {
        return 0;
    }

    return (pte & 0xFFFFF000) + (vaddr & 0xFFF);
}

static bool vmm_is_mapped(uint32_t vaddr) {
    uint32_t* pd = (uint32_t*)0xFFFFF000;
    uint32_t pde = pd[vaddr >> PD_INDEX_SHIFT];

    if (!(pde & PAGE_PRESENT))
        return false;

    uint32_t* pt_window = (uint32_t*)0xFFC00000;
    uint32_t pte = pt_window[vaddr >> PT_INDEX_SHIFT];

    return (pte & PAGE_PRESENT);
}

/* ---------------- mapping / unmapping ---------------- */
static inline void vmm_set_pte_at(uint32_t window_base, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t* pt_window = (uint32_t*)window_base;
    uint32_t page_num = vaddr >> 12;

    pt_window[page_num] = (paddr & 0xFFFFF000) | flags;

    if (window_base == 0xFFC00000) {
        asm volatile("invlpg (%0)" : : "r" (vaddr) : "memory");
    }
}

void vmm_map_kernel_page(uint32_t vaddr, uint32_t phys_addr) {
    vmm_set_pte_at(0xFFC00000, vaddr, phys_addr, PAGE_PRESENT | PAGE_RW | PAGE_GLOBAL);
}

void vmm_unmap_kernel_page(uint32_t vaddr) {
    vmm_set_pte_at(0xFFC00000, vaddr, 0, 0);
}

/* ---------------- allocation ---------------- */
uintptr_t vmm_alloc_kernel_page(){
    // Check for entry in free list
    if (free_list.count > 0) {
        free_list_node_t* node = free_list_pop(&free_list);
        return  (uintptr_t)node;
    }

    // Clamp last free
    if (last_free < KERNEL_HIGHER_HALF || last_free >= MEMORY_SPACE) {
        last_free = KERNEL_HIGHER_HALF;
    }

    uintptr_t start = PAGE_ALIGN_UP(last_free);
    uintptr_t i = start;

    do {
        if (i >= MEMORY_SPACE) {
            i = KERNEL_HIGHER_HALF;
        }

        if (!vmm_is_mapped((uint32_t)i)) {
            uint32_t phys = pmm_alloc_page();
            if (!phys) {
                printf("vmm: failed to allocate physical page\n");
                return 0;
            }
            printf("vmm: Allocated page at virt: %x, phys: %x\n", (unsigned int)i, phys);
            vmm_map_kernel_page((uint32_t)i, phys);
            memset((void*)i, 0, PAGE_SIZE);
            last_free = i + PAGE_SIZE;
            return i;
        }

        i += PAGE_SIZE;
    } while (i != start);

    printf("vmm: no free kernel pages [%x-%x]\n",
            (unsigned int)KERNEL_HIGHER_HALF, (unsigned int)MEMORY_SPACE-1);
    return 0;
}

void vmm_free_kernel_page(uintptr_t addr) {
    free_list_node_t* node = (free_list_node_t*)addr;
    free_list_add(&free_list, node);

    while (free_list.count > VMM_FREE_LIST_MAX_SIZE) {
        free_list_node_t* to_free = free_list_pop(&free_list);
        if (!to_free)
            break;
        uintptr_t vaddr = (uintptr_t)to_free;
        uint32_t phys = vmm_virt_to_phys((uint32_t)vaddr);
        if (phys) {
            vmm_unmap_kernel_page((uint32_t)vaddr);
            pmm_free_page(phys);
        }
    }
}

/* ---------------- address-space creation ---------------- */

page_directory_t* vmm_create_address_space(void) {
    printf("Not Implemented Yet");
    /*
    if (!kernel_pd) {
        printf("vmm_create_address_space: kernel_pd not initialized\n");
        return NULL;
    }

    uintptr_t new_pd_virt = vmm_alloc_page(kernel_pd);
    if (!new_pd_virt) {
        printf("vmm_create_address_space: failed to allocate PD page\n");
        return NULL;
    }

    uint32_t new_pd_phys = vmm_virt_to_phys(kernel_pd, (uint32_t)new_pd_virt);
    if (!new_pd_phys) {
        printf("vmm_create_address_space: failed to resolve PD phys\n");
        return NULL;
    }

    memset((void*)new_pd_virt, 0, PAGE_SIZE);

    uint32_t kernel_pde_start = (uint32_t)(KERNEL_HIGHER_HALF >> PD_INDEX_SHIFT);
    if (kernel_pde_start > 1024) kernel_pde_start = 768;

    for (int i = (int)kernel_pde_start; i < 1024; i++) {
        ((uint32_t*)new_pd_virt)[i] = kernel_pd->virt[i];
    }

    uintptr_t meta_virt = vmm_alloc_page(kernel_pd);
    if (!meta_virt) {
        printf("vmm_create_address_space: failed to allocate metadata page\n");
        return NULL;
    }

    page_directory_t* pd_struct = (page_directory_t*)meta_virt;
    pd_struct->virt = (uint32_t*)new_pd_virt;
    pd_struct->phys = new_pd_phys;
    pd_struct->is_kernel = 0;

    return pd_struct;
    */
}