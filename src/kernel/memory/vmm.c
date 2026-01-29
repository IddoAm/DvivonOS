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


/* ---------------- vmm init / switch ---------------- */

void vmm_init(void) {
    // Nothing needed here for now
}

void vmm_switch_address_space(uint32_t phys_addr) {
    if (!phys_addr)
        return;
    vmm_write_cr3(phys_addr);
    vmm_flush_cr3();
}

/* ---------------- address translation / is_mapped ---------------- */

static uint32_t vmm_virt_to_phys(uint32_t vaddr) {
    uint32_t* pt_window = (uint32_t*)PTS_WINDOW;
    uint32_t* pd_window = (uint32_t*)PD_WINDOW;

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
static inline void vmm_set_pde_at(uint32_t window_base, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t* pd_window = (uint32_t*)window_base;
    uint32_t pd_num = vaddr >> PD_INDEX_SHIFT;

    pd_window[pd_num] = (paddr & 0xFFFFF000) | flags;
}

static inline void vmm_set_pte_at(uint32_t window_base, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t* pt_window = (uint32_t*)window_base;
    uint32_t page_num = vaddr >> PT_INDEX_SHIFT;

    pt_window[page_num] = (paddr & 0xFFFFF000) | flags;

    if (window_base == PTS_WINDOW) {
        vmm_invlpg((void*)vaddr);
    }
}

static inline void vmm_map_kernel_page(uint32_t vaddr, uint32_t phys_addr) {
    vmm_set_pte_at(PTS_WINDOW, vaddr, phys_addr, PAGE_PRESENT | PAGE_RW | PAGE_GLOBAL);
}

static inline void vmm_map_user_page(uint32_t vaddr, uint32_t phys_addr) {
    uint32_t pd_idx = vaddr >> 22; // The PD index determines which PT we are talking about
    uint32_t* pd = (uint32_t*)PD_WINDOW;

    if ((pd[pd_idx] & PAGE_PRESENT) == 0) {
        printf("vmm: allocating page table for vaddr %x (PD index %d)\n", vaddr, pd_idx);
        uint32_t new_pt_phys = pmm_alloc_page();
        if (!new_pt_phys) return;

        vmm_set_pde_at(PD_WINDOW, vaddr, new_pt_phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);

        // 1. Calculate the virtual address of the PT in the recursive map
        // Each PT is one PAGE_SIZE (4KB) inside the PTS_WINDOW
        void* pt_vaddr = (void*)(PTS_WINDOW + (pd_idx * PAGE_SIZE));

        // 2. CRITICAL: Invalidate the PT's virtual address, NOT the user vaddr
        vmm_invlpg(pt_vaddr);

        // 3. Zero out the PT
        memset(pt_vaddr, 0, PAGE_SIZE);
    }
    printf("Allocating user page at virt: %x, phys: %x\n", (unsigned int)vaddr, phys_addr);
    vmm_set_pte_at(PTS_WINDOW, vaddr, phys_addr, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    printf("Success\n");
}

static inline void vmm_unmap_page(uint32_t vaddr) {
    vmm_set_pte_at(PTS_WINDOW, vaddr, 0, 0);
}

/* ---------------- kernel allocation ---------------- */
bool vmm_alloc_kernel_page_at(uint32_t vaddr){
    uint32_t phys = pmm_alloc_page();
    if (!phys) {
        printf("vmm: failed to allocate physical page\n");
        return false;
    }
    vmm_map_kernel_page((uint32_t)vaddr, phys);
    memset((void*)vaddr, 0, PAGE_SIZE);
    return true;
}

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
            if(vmm_alloc_kernel_page_at(i)){
               return i;
            }
        }

        i += PAGE_SIZE;
    } while (i != start);

    printf("vmm: no free kernel pages [%x-%x]\n",
            (unsigned int)KERNEL_HIGHER_HALF, (unsigned int)MEMORY_SPACE-1);
    return 0;
}

void vmm_free_kernel_page(uint32_t vaddr) {
    if(vaddr % PAGE_SIZE != 0) return;
    
    free_list_node_t* node = (free_list_node_t*)vaddr;
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

/* ---------------- userspace allocation ---------------- */

bool vmm_alloc_user_page_at(uint32_t vaddr) {
    uint32_t phys = pmm_alloc_page();
    if (!phys) {
        printf("vmm: failed to allocate physical page\n");
        return false;
    }

    vmm_map_user_page(vaddr, phys);
    memset((void*)vaddr, 0, PAGE_SIZE);
    return true;
}

void vmm_free_user_page(uint32_t vaddr) {
    uint32_t paddr = vmm_virt_to_phys(vaddr);
    if (paddr) {
        vmm_unmap_page(vaddr);
        pmm_free_page(paddr);
    }
}


/* ---------------- address-space creation ---------------- */

// Return physical address of created pd
uint32_t vmm_create_address_space(void) {
    uint32_t new_pd_phys = pmm_alloc_page();
    if (!new_pd_phys) return 0;
    printf("goo");
    // 1. Map the new PD into our "Scratch" slot (1022)
    // We don't use vmm_set_pde_at with PD_SCRATCH_WINDOW because that macro 
    // is the TARGET address, not the VADDR we are mapping.
    uint32_t* current_pd = (uint32_t*)PD_WINDOW;
    printf("goo");
    // Per your instructions: Kernel allocations use the GLOBAL flag (bit 8)
    current_pd[1022] = new_pd_phys | PAGE_PRESENT | PAGE_RW | PAGE_GLOBAL;
    printf("goo");
    // 2. Flush the TLB so the CPU sees the new mapping at 0xFFFFE000
    vmm_invlpg((void*)PD_SCRATCH_WINDOW);
    printf("goo");
    uint32_t* new_pd = (uint32_t*)PD_SCRATCH_WINDOW;

    // 3. Clear the user-space part (0-767) to ensure it's empty
    for (uint32_t i = 0; i < 768; i++) {
        new_pd[i] = 0;
    }
    printf("goo");
    // 4. Copy kernel PDEs (768-1021)
    for (uint32_t i = 768; i < 1022; i++) {
        new_pd[i] = current_pd[i];
    }
    printf("goo");
    // 5. Setup self-reference for the NEW PD at its own index 1023
    // This must also be GLOBAL as it's a kernel-level structural mapping.
    new_pd[1023] = new_pd_phys | PAGE_PRESENT | PAGE_RW | PAGE_GLOBAL;
    printf("goo");
    // 6. Clean up: Unmap from scratch slot (optional but cleaner)
    current_pd[1022] = 0;
    printf("goo");
    vmm_invlpg((void*)PD_SCRATCH_WINDOW);
    printf("goo");
    return new_pd_phys;
}

void vmm_destroy_address_space(uint32_t pd_phys) {
    vmm_set_pde_at(PD_WINDOW, PD_SCRATCH_WINDOW, pd_phys, PAGE_PRESENT | PAGE_RW);


    uint32_t* victim_pd = (uint32_t*)PD_SCRATCH_WINDOW; // The PD via recursive mapping
    uint32_t* victim_pts = (uint32_t*)PTS_SCRATCH_WINDOW; // The PTs via recursive mapping

    // Loop through User-space entries only
    for (uint32_t i = 0; i < 768; i++) {
        if (victim_pd[i] & PAGE_PRESENT) {
            uint32_t* pt = &victim_pts[i * 1024];

            for (uint32_t j = 0; j < 1024; j++) {
                if (pt[j] & PAGE_PRESENT) {
                    pmm_free_page(pt[j] & 0xFFFFF000);
                }
            }

            pmm_free_page(victim_pd[i] & 0xFFFFF000);
        }
    }

    vmm_set_pte_at(PD_WINDOW, PD_SCRATCH_WINDOW, 0, 0); 
    pmm_free_page(pd_phys);
}