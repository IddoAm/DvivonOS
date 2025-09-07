
#include <kernel/vmm.h>
#include <kernel/memory_defs.h>
#include <kernel/pmm.h>
#include <lib/stdio.h>
#include <lib/string.h>>

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
    for (uint32_t t = 0; t < identity_tables; t++) {
        set_page_entry(&_page_directory_start[HH_PDE + t],
                       (uint32_t)_page_tables_start + t * PAGE_SIZE,
                       PAGE_PRESENT | PAGE_RW);
    }
    
    // Step 4: enable paging
    enable_paging((uint32_t)_page_directory_start, KERNEL_HIGHER_HALF);

    // TODO: Remove the identity mapping

    paging_enabled = true;
    adjust_bitmap_address_for_paging();
}
