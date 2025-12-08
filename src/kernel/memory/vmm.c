// kernel/vmm.c
#include <kernel/memory_defs.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* Keep kernel-only free-list & last_free behaviour from the original implementation */
static free_list_t free_list = {0};
static uintptr_t last_free = KERNEL_HIGHER_HALF;

/* NOTE:
 * header defines `static page_directory_t* kernel_pd = NULL;`
 * (you provided the header with that `static` variable).
 * We rely on that symbol being available in this translation unit.
 */

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

/* Return the global kernel page-directory object (created in vmm_init). */
page_directory_t* vmm_get_kernel_pd(void) {
    return kernel_pd;
}

/* ---------------- vmm init / switch ---------------- */

uintptr_t vmm_alloc_page(page_directory_t* pd) {
    if (!pd) return 0;

    uintptr_t alloc_start, alloc_end;
    
    if (pd->is_kernel) {
        alloc_start = KERNEL_HIGHER_HALF;
        alloc_end = (uintptr_t)MEMORY_SPACE;
    } else {
        alloc_start = 0x00100000;
        alloc_end = KERNEL_HIGHER_HALF;
    }

    if (pd != kernel_pd) {
        printf("vmm_alloc_page: only kernel PD supported for automatic allocation\n");
        return 0;
    }

    if (free_list.count > 0) {
        free_list_node_t* n = free_list_pop(&free_list);
        uintptr_t addr = (uintptr_t)n;
        
        if (addr >= alloc_start && addr < alloc_end) {
            return addr;
        } else {
            printf("vmm: free list entry %x outside valid range [%x-%x]\n", 
                   addr, alloc_start, alloc_end);
        }
    }

    if (last_free < alloc_start || last_free >= alloc_end) {
        last_free = alloc_start;
    }

    uintptr_t start = PAGE_ALIGN_UP(last_free);
    uintptr_t i = start;
    
    do {
        if (i >= alloc_end) {
            i = alloc_start;
            if (i >= start) {
                printf("vmm: no free pages in valid range [%x-%x]\n", 
                       alloc_start, alloc_end);
                return 0;
            }
        }
        
        if (!vmm_is_mapped_in(pd, (uint32_t)i)) {
            uint32_t phys = pmm_alloc_page();
            if (!phys) {
                printf("vmm: pmm_alloc_page() returned 0\n");
                return 0;
            }
            printf("Allocated page at virt: %x, phys: %x\n", i, phys);
            vmm_map_page(pd, (uint32_t)i, phys, PAGE_PRESENT | PAGE_RW);
            memset((void*)i, 0, PAGE_SIZE);
            last_free = i + PAGE_SIZE;
            return i;
        }
        
        i += PAGE_SIZE;
        
        if (i >= alloc_end) {
            i = alloc_start;
        }
    } while (i != start);

    printf("vmm: address space exhausted in range [%x-%x]\n", 
           alloc_start, alloc_end);
    return 0;
}

/* Unmap a virtual page from a specific page directory */
void vmm_unmap_page(page_directory_t* pd, uint32_t vaddr) {
    if (!pd || !pd->virt)
        return;

    uint32_t pdi = pd_index(vaddr);
    uint32_t pti = pt_index(vaddr);

    uint32_t pde = pd->virt[pdi];
    if (!(pde & PAGE_PRESENT))
        return;

    uint32_t pt_phys = pde & PAGE_FRAME_MASK;
    uint32_t* pt_virt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)pt_phys);

    pt_virt[pti] = 0;
    vmm_invlpg((void*)(uintptr_t)vaddr);
}

/* ---------------- allocation (kernel-only) ---------------- */

/* Generic API but currently implemented for kernel page directory only.
 * If pd != kernel_pd the function will return 0; later you can add per-PD
 * allocation state (heap pointer) and free-list for users.
 */
uintptr_t vmm_alloc_page(page_directory_t* pd) {
    if (!pd) return 0;

    if (pd != kernel_pd) {
        /* For now we don't maintain per-address-space last_free pointer.
         * User-space heaps should be managed by the heap subsystem which
         * will call vmm_map_page with the appropriate addresses.
         */
        printf("vmm_alloc_page: only kernel PD supported for automatic allocation\n");
        return 0;
    }

    /* kernel behaviour preserved from original implementation */
    if (free_list.count > 0) {
        free_list_node_t* n = free_list_pop(&free_list);
        return (uintptr_t)n;
    }

    uintptr_t start = PAGE_ALIGN_UP(last_free);
    uintptr_t i = start;
    do {
        if (!vmm_is_mapped_in(pd, (uint32_t)i)) {
            uint32_t phys = pmm_alloc_page();
            if (!phys) {
                printf("vmm: pmm_alloc_page() returned 0\n");
                return 0;
            }
            printf("Allocated page at phys: %x\n", phys);
            vmm_map_page(pd, (uint32_t)i, phys, PAGE_PRESENT | PAGE_RW);
            memset((void*)i, 0, PAGE_SIZE);
            last_free = i + PAGE_SIZE;
            return i;
        }
        i += PAGE_SIZE;
        if (i >= (uintptr_t)MEMORY_SPACE)
            i = (uintptr_t)KERNEL_HIGHER_HALF;
    } while (i != start);

    return 0;
}

/* Free a kernel virtual page: add to free list; when free list grows, actually free physical pages */
void vmm_free_page(page_directory_t* pd, uintptr_t addr) {
    if (pd != kernel_pd) {
        printf("vmm_free_page: only kernel PD supported for automatic free\n");
        return;
    }

    free_list_node_t* node = (free_list_node_t*)addr;
    free_list_add(&free_list, node);

    while (free_list.count > VMM_FREE_LIST_MAX_SIZE) {
        free_list_node_t* to_free = free_list_pop(&free_list);
        if (!to_free)
            break;
        uintptr_t vaddr = (uintptr_t)to_free;
        uint32_t phys = vmm_virt_to_phys(pd, (uint32_t)vaddr);
        if (phys) {
            vmm_unmap_page(pd, (uint32_t)vaddr);
            pmm_free_page(phys);
        }
    }
}

/* ---------------- address-space creation ---------------- */

/* Create a new address space object with kernel high-half copied.
 * Returns a heap-allocated page_directory_t* (caller should free via pmm or keep pointer).
 */
page_directory_t* vmm_create_address_space(void) {
    if (!kernel_pd) {
        printf("vmm_create_address_space: kernel_pd not initialized\n");
        return NULL;
    }

    /* allocate virtual page for the new page directory using kernel allocator */
    uintptr_t new_pd_virt = vmm_alloc_page(kernel_pd);
    if (!new_pd_virt) {
        printf("vmm_create_address_space: failed to allocate PD page\n");
        return NULL;
    }

    /* new_pd_virt is a kernel-virtual address mapped by kernel_pd; find its physical */
    uint32_t new_pd_phys = vmm_virt_to_phys(kernel_pd, (uint32_t)new_pd_virt);
    if (!new_pd_phys) {
        printf("vmm_create_address_space: failed to resolve PD phys\n");
        return NULL;
    }

    /* clear the new page directory */
    memset((void*)new_pd_virt, 0, PAGE_SIZE);

    /* copy kernel entries (higher-half). The range below was previously 758..1024.
     * We'll copy PDEs from kernel's PD where they're present.
     * The exact start index for kernel region depends on KERNEL_HIGHER_HALF; compute it.
     */
    uint32_t kernel_pde_start = (uint32_t)(KERNEL_HIGHER_HALF >> PD_INDEX_SHIFT);
    if (kernel_pde_start > 1024) kernel_pde_start = 758; /* fallback to previous constant */

    for (int i = kernel_pde_start; i < 1024; i++) {
        uint32_t entry = kernel_pd->virt[i];
        if (entry & PAGE_PRESENT) {
            ((uint32_t*)new_pd_virt)[i] = entry;
        }
    }

    /* allocate a small page_directory_t for this address space on kernel heap (pmm-backed)
     * We don't have a general-purpose small-kernel allocator here, so allocate memory
     * using kernel vmm_alloc_page to hold the struct (wasteful but simple).
     */
    uintptr_t meta_virt = vmm_alloc_page(kernel_pd);
    if (!meta_virt) {
        printf("vmm_create_address_space: failed to allocate metadata page\n");
        /* Note: we won't free new_pd_virt here; user can clean if desired */
        return NULL;
    }

    page_directory_t* pd_struct = (page_directory_t*)meta_virt;
    pd_struct->virt = (uint32_t*)new_pd_virt;
    pd_struct->phys = new_pd_phys;
    pd_struct->is_kernel = 0;

    return pd_struct;
}
