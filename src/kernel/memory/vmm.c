// kernel/vmm.c
#include <kernel/memory-defs.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <lib/stdio.h>
#include <lib/string.h>

static free_list_t free_list = {0};
static uintptr_t last_free = KERNEL_HIGHER_HALF;

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

uint32_t vmm_virt_to_phys(uint32_t vaddr) {
    uint32_t pdi = pd_index(vaddr);
    uint32_t pti = pt_index(vaddr);
    uint32_t off = vaddr & PAGE_OFFSET_MASK;

    uint32_t pde = _page_directory_start[pdi];
    if (!(pde & PAGE_PRESENT))
        return 0;

    uint32_t pt_phys = pde & PAGE_FRAME_MASK;
    uint32_t* pt_virt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)pt_phys);
    uint32_t pte = pt_virt[pti];
    if (!(pte & PAGE_PRESENT))
        return 0;

    return (pte & PAGE_FRAME_MASK) + off;
}

int vmm_is_mapped(uint32_t vaddr) {
    uint32_t pde = _page_directory_start[pd_index(vaddr)];
    if (!(pde & PAGE_PRESENT))
        return 0;
    uint32_t* pt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)(pde & PAGE_FRAME_MASK));
    return (pt[pt_index(vaddr)] & PAGE_PRESENT) ? 1 : 0;
}

/* Map vaddr -> phys (create PT if needed). vaddr & phys_addr must be page-aligned. */
void vmm_map_kernel_page(uint32_t vaddr, uint32_t phys_addr, uint32_t flags) {
    uint32_t pdi = pd_index(vaddr);
    uint32_t pti = pt_index(vaddr);

    uint32_t pde = _page_directory_start[pdi];
    uint32_t* pt_virt;

    if (!(pde & PAGE_PRESENT)) {
        uint32_t new_pt_phys = pmm_alloc_page();
        if (!new_pt_phys) {
            printf("vmm: pmm_alloc_page() failed\n");
            return;
        }
        _page_directory_start[pdi] = (new_pt_phys & PAGE_FRAME_MASK) | (PAGE_PRESENT | PAGE_RW);
        pt_virt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)new_pt_phys);
        memset(pt_virt, 0, PAGE_SIZE);
    } else {
        uint32_t pt_phys = pde & PAGE_FRAME_MASK;
        pt_virt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)pt_phys);
    }

    pt_virt[pti] = (phys_addr & PAGE_FRAME_MASK) | (flags & PAGE_FLAGS_MASK);
    vmm_invlpg((void*)(uintptr_t)vaddr);
}

void vmm_unmap_page(uint32_t vaddr) {
    uint32_t pdi = pd_index(vaddr);
    uint32_t pti = pt_index(vaddr);

    uint32_t pde = _page_directory_start[pdi];
    if (!(pde & PAGE_PRESENT))
        return;

    uint32_t pt_phys = pde & PAGE_FRAME_MASK;
    uint32_t* pt_virt = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)pt_phys);

    pt_virt[pti] = 0;
    vmm_invlpg((void*)(uintptr_t)vaddr);
}

/* free-list ops */
void free_list_add(free_list_t* list, free_list_node_t* node) {
    node->next = list->head;
    node->prev = NULL;
    if (list->head)
        list->head->prev = node;
    else
        list->tail = node;
    list->head = node;
    list->count++;
}

free_list_node_t* free_list_pop(free_list_t* list) {
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

uintptr_t kernel_vmm_alloc_page(void) {
    if (free_list.count > 0) {
        free_list_node_t* n = free_list_pop(&free_list);
        return (uintptr_t)n;
    }

    uintptr_t start = PAGE_ALIGN_UP(last_free);
    uintptr_t i = start;
    do {
        if (!vmm_is_mapped((uint32_t)i)) {
            uint32_t phys = pmm_alloc_page();
            printf("Allocated page at phys: %x\n", phys);
            // if (!phys) return 0;
            vmm_map_kernel_page((uint32_t)i, phys, PAGE_PRESENT | PAGE_RW);
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

void kernel_vmm_free_page(uintptr_t addr) {
    free_list_node_t* node = (free_list_node_t*)addr;
    free_list_add(&free_list, node);

    while (free_list.count > VMM_FREE_LIST_MAX_SIZE) {
        free_list_node_t* to_free = free_list_pop(&free_list);
        if (!to_free)
            break;
        uintptr_t vaddr = (uintptr_t)to_free;
        uint32_t phys = vmm_virt_to_phys((uint32_t)vaddr);
        if (phys) {
            vmm_unmap_page((uint32_t)vaddr);
            pmm_free_page(phys);
        }
    }
}
