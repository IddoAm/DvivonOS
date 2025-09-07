#include <kernel/vmm.h>
#include <kernel/memory_defs.h>
#include <kernel/pmm.h>
#include <lib/stdio.h>
#include <lib/string.h>>

static uint32_t* page_directory_hh;

void vmm_init(void)
{
    const uint32_t phys_base = (uint32_t)_kernel_start; 
    const uint32_t phys_end  = (uint32_t)_kernel_end;

    // Step 0: identity range
    const uint32_t identity_end    = (phys_end + PAGE_TABLE_SIZE - 1) & ~(PAGE_TABLE_SIZE - 1);
    const uint32_t identity_pages  = identity_end / PAGE_SIZE;
    const uint32_t identity_tables = (identity_pages + PT_ENTRIES - 1) / PT_ENTRIES;

    // Step 1: fill identity PTEs
    for (uint32_t i = 0; i < identity_pages; i++) {
        set_page_entry(&_page_tables_start[i], i * PAGE_SIZE, PAGE_PRESENT | PAGE_RW);
    }

    // Step 2: link PDEs for identity
    for (uint32_t t = 0; t < identity_tables; t++) {
        set_page_entry(&_page_directory_start[t],
                       (uint32_t)_page_tables_start + t * PAGE_SIZE,
                       PAGE_PRESENT | PAGE_RW);
    }


    // Step 3: mirror into higher-half
    const uint32_t HH_PDE = PD_ENTRIES - PAGE_TABLE_COUNT; // 768
    for (uint32_t t = 0; t < PAGE_TABLE_COUNT; t++) {
        set_page_entry(&_page_directory_start[HH_PDE + t],
                       (uint32_t)_page_tables_start + t * PAGE_SIZE,
                       PAGE_PRESENT | PAGE_RW);
    }
    
    // Step 4: enable paging
    enable_paging((uint32_t)_page_directory_start, KERNEL_HIGHER_HALF);
    page_directory_hh = (uint32_t*)(KERNEL_HIGHER_HALF + (uintptr_t)_page_directory_start);

    // TODO: Remove the identity mapping

    paging_enabled = true;
    //adjust_bitmap_address_for_paging();
}


uint32_t vmm_virt_to_phys(uint32_t vaddr) {
    uint32_t pd_index = (vaddr >> 22) & 0x3FF;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;
    uint32_t page_offset = vaddr & 0xFFF;

    // Access PDE through higher-half page directory pointer
    uint32_t pde = page_directory_hh[pd_index];
    if (!(pde & PAGE_PRESENT)) return 0;  // not mapped

    // Physical address of page table
    uint32_t pt_phys = pde & 0xFFFFF000;

    // Access PT via higher-half mapping (assuming you have PTs mirrored in HH)
    uint32_t* pt = (uint32_t*)(KERNEL_HIGHER_HALF + pt_phys);
    uint32_t pte = pt[pt_index];
    if (!(pte & PAGE_PRESENT)) return 0;  // not mapped

    // Return physical address of the actual page + offset
    return (pte & 0xFFFFF000) + page_offset;
}

static inline int vmm_is_mapped(uint32_t vaddr) {
    uint32_t pd_index = (vaddr >> 22) & 0x3FF;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;

    uint32_t pde = page_directory_hh[pd_index];
    if (!(pde & PAGE_PRESENT)) return 0;

    uint32_t* pt = (uint32_t*)(KERNEL_HIGHER_HALF + (pde & 0xFFFFF000));
    return (pt[pt_index] & PAGE_PRESENT) ? 1 : 0;
}

static inline void vmm_map_kernel_page(uint32_t vaddr, uint32_t phys_addr, uint32_t flags) {
    uint32_t pd_index = (vaddr >> 22) & 0x3FF;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;

    uint32_t pde = page_directory_hh[pd_index];
    uint32_t* pt;
    if (!(pde & PAGE_PRESENT)) {
        // Allocate a page for the page table
        uint32_t pt_phys = pmm_alloc_page();
        page_directory_hh[pd_index] = pt_phys | PAGE_PRESENT | PAGE_RW;
        pt = (uint32_t*)(KERNEL_HIGHER_HALF + pt_phys);
        memset(pt, 0, PAGE_SIZE);
    } else {
        uint32_t pt_phys = pde & 0xFFFFF000;
        pt = (uint32_t*)(KERNEL_HIGHER_HALF + pt_phys);
    }

    pt[pt_index] = phys_addr | flags;
    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

static inline void vmm_map_kernel_hh(uint32_t vaddr, uint32_t phys_addr, uint32_t flags) {
    // Derive PD and PT indices
    uint32_t pd_index = (vaddr >> 22) & 0x3FF;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;

    // Kernel higher-half PDEs start at 768
    const uint32_t hh_base_pd_index = PD_ENTRIES - PAGE_TABLE_COUNT; // 1024 - 256 = 768

    // Calculate PT offset into the linker-allocated .page_tables
    uint32_t pt_offset = pd_index - hh_base_pd_index;

    // Direct pointer into the preallocated tables
    uint32_t* pt = &_page_tables_start[pt_offset * PT_ENTRIES];

    // Write the PTE
    pt[pt_index] = phys_addr | flags;

    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

static inline void vmm_unmap_page(uint32_t vaddr) {
    uint32_t pd_index = (vaddr >> 22) & 0x3FF;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;

    uint32_t pde = page_directory_hh[pd_index];
    if (!(pde & PAGE_PRESENT)) return; // Page table not present

    uint32_t pt_phys = pde & 0xFFFFF000;
    uint32_t* pt = (uint32_t*)(KERNEL_HIGHER_HALF + pt_phys);

    pt[pt_index] = 0; // Unmap the page

    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

static free_list_node_t* head; 
static free_list_node_t* tail; 

static uintptr_t last_free = KERNEL_HIGHER_HALF;

uintptr_t kernel_vmm_alloc_page(){
    uintptr_t i = last_free;
    do {
        if (!vmm_is_mapped(i)) {
            uintptr_t phys_addr = pmm_alloc_page();
            printf("Allocating phys_addr: %x for vaddr: %x\n", phys_addr, i);
            vmm_map_kernel_hh(i, phys_addr, PAGE_PRESENT | PAGE_RW);
            last_free = i;
            return i;
        }

        i += PAGE_SIZE;
        if (i >= MEMORY_SPACE) i = KERNEL_HIGHER_HALF;

    } while (i != last_free - PAGE_SIZE);

    return 0; // Failed to allocate
    
}

void kernel_vvmm_free_page(const uintptr_t addr){
    uintptr_t phys_addr = vmm_virt_to_phys(addr);
    pmm_free_page(phys_addr);
    vmm_unmap_page(addr);
}