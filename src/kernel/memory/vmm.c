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

uint32_t vmm_virt_to_phys(page_directory_t* pd, uint32_t vaddr) {
    if (!pd || !pd->virt)
        return 0;

    uint32_t pdi = pd_index(vaddr);
    uint32_t pti = pt_index(vaddr);
    uint32_t off = vaddr & PAGE_OFFSET_MASK;

    uint32_t pde = pd->virt[pdi];
    if (!(pde & PAGE_PRESENT))
        return 0;

    uint32_t pt_phys = pde & PAGE_FRAME_MASK;
    uint32_t* pt_virt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)pt_phys);
    uint32_t pte = pt_virt[pti];
    if (!(pte & PAGE_PRESENT))
        return 0;

    return (pte & PAGE_FRAME_MASK) + off;
}

static int vmm_is_mapped_in(page_directory_t* pd, uint32_t vaddr) {
    if (!pd || !pd->virt)
        return 0;
    uint32_t pde = pd->virt[pd_index(vaddr)];
    if (!(pde & PAGE_PRESENT))
        return 0;
    uint32_t* pt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)(pde & PAGE_FRAME_MASK));
    return (pt[pt_index(vaddr)] & PAGE_PRESENT) ? 1 : 0;
}

/* ---------------- mapping / unmapping ---------------- */

void vmm_map_page(page_directory_t* pd, uint32_t vaddr, uint32_t phys_addr, uint32_t flags) {
    if (!pd || !pd->virt) {
        printf("vmm: vmm_map_page: null pd\n");
        return;
    }

    uint32_t pdi = pd_index(vaddr);
    uint32_t pti = pt_index(vaddr);

    uint32_t pde = pd->virt[pdi];
    uint32_t* pt_virt;

    if (!(pde & PAGE_PRESENT)) {
        uint32_t new_pt_phys = pmm_alloc_page();
        if (!new_pt_phys) {
            printf("vmm: pmm_alloc_page() failed while creating PT\n");
            return;
        }

        uint32_t pde_flags = PAGE_PRESENT | PAGE_RW;
        if (flags & PAGE_USER)
            pde_flags |= PAGE_USER;

        pd->virt[pdi] = (new_pt_phys & PAGE_FRAME_MASK) | (pde_flags & PAGE_FLAGS_MASK);

        pt_virt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)new_pt_phys);
        memset(pt_virt, 0, PAGE_SIZE);
    } else {
        uint32_t pt_phys = pde & PAGE_FRAME_MASK;
        pt_virt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)pt_phys);
    }

    pt_virt[pti] = (phys_addr & PAGE_FRAME_MASK) | (flags & PAGE_FLAGS_MASK);
    vmm_invlpg((void*)(uintptr_t)vaddr);
}

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

/* ---------------- allocation ---------------- */

uintptr_t vmm_alloc_page(page_directory_t* pd) {
    if (!pd) return 0;

    uintptr_t alloc_start, alloc_end;
    if (pd->is_kernel) {
        alloc_start = KERNEL_HIGHER_HALF;
        alloc_end = MEMORY_SPACE - 1;
    } else {
        alloc_start = 0x00100000;
        alloc_end = KERNEL_HIGHER_HALF;
    }

    /* Kernel PD: keep existing free-list / last_free behavior */
    if (pd == kernel_pd) {
        if (free_list.count > 0) {
            free_list_node_t* n = free_list_pop(&free_list);
            uintptr_t addr = (uintptr_t)n;
            if (addr >= alloc_start && addr < alloc_end) {
                return addr;
            } else {
                printf("vmm: free list entry %x outside valid range [%x-%x]\n",
                       (unsigned int)addr, (unsigned int)alloc_start, (unsigned int)alloc_end);
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
                           (unsigned int)alloc_start, (unsigned int)alloc_end);
                    return 0;
                }
            }

            if (!vmm_is_mapped_in(pd, (uint32_t)i)) {
                uint32_t phys = pmm_alloc_page();
                if (!phys) {
                    printf("vmm: pmm_alloc_page() returned 0\n");
                    return 0;
                }
                printf("Allocated page at virt: %x, phys: %x\n", (unsigned int)i, phys);
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
               (unsigned int)alloc_start, (unsigned int)alloc_end);
        return 0;
    }

    /* Non-kernel PD: allocate a physical page and map it into the target PD */
    {
        uintptr_t start = PAGE_ALIGN_UP(alloc_start);
        uintptr_t i = start;

        do {
            if (i >= alloc_end) {
                i = alloc_start;
                if (i >= start) {
                    printf("vmm: no free pages for user PD in range [%x-%x]\n",
                           (unsigned int)alloc_start, (unsigned int)alloc_end);
                    return 0;
                }
            }

            if (!vmm_is_mapped_in(pd, (uint32_t)i)) {
                uint32_t phys = pmm_alloc_page();
                if (!phys) {
                    printf("vmm: pmm_alloc_page() returned 0 for user PD\n");
                    return 0;
                }
                /* Map with user permissions */
                vmm_map_page(pd, (uint32_t)i, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
                /* The kernel is currently running with kernel_cr3 — 'i' is not mapped
                   in the kernel address space. Zero the physical page via the kernel's
                   direct-mapping (KERNEL_HIGHER_HALF + phys) instead. */
                memset((void*)(KERNEL_HIGHER_HALF + (uintptr_t)phys), 0, PAGE_SIZE);
                return i;
            }

            i += PAGE_SIZE;
        } while (i != start);

        printf("vmm: user address space exhausted in range [%x-%x]\n",
               (unsigned int)alloc_start, (unsigned int)alloc_end);
        return 0;
    }
}

int vmm_alloc_page_at(page_directory_t* pd, uint32_t vaddr, uint32_t flags) {
    uint32_t phys = pmm_alloc_page();
    if (!phys) return -1;
    vmm_map_page(pd, vaddr, phys, flags);
    /* Zero the page using kernel mapping of the physical frame */
    memset((void*)(KERNEL_HIGHER_HALF + (uintptr_t)phys), 0, PAGE_SIZE);
    return 0;
}

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

page_directory_t* vmm_create_address_space(void) {
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
}