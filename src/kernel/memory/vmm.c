
#include <kernel/vmm.h>
#include <kernel/memory_defs.h>
#include <kernel/pmm.h>
#include <lib/stdio.h>

void vmm_init() {
    uint32_t page_directory_start_index = PAGE_DIR_TABLE_SIZE - PAGE_TABLE_COUNT; // 768
    uint32_t kernel_send_pages = ((uintptr_t)_kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;

    /*
    for(int i = page_directory_start_index; i < page_directory_start_index + kernel_size_pages; i++) {
        uint32_t phys_addr = (uint32_t)_kernel_start + (i*PAGE_SIZE);
        set_page_entry(&_page_tables_start[i-page_directory_start_index], phys_addr, PAGE_PRESENT | PAGE_RW);
    }
    */

    // Fill page tables 
    for(int i = 0; i < kernel_send_pages; i++) {
        uint32_t phys_addr = i * PAGE_SIZE;
        set_page_entry(&_page_tables_start[i], phys_addr, PAGE_PRESENT | PAGE_RW);
    }

    // Fill page diretory identity
    for(int i = 0; i < PAGE_TABLE_COUNT; i++){
        uint32_t phys_addr = (uint32_t)_page_tables_start + (i*PAGE_SIZE);
        set_page_entry(&_page_directory_start[i], phys_addr, PAGE_PRESENT | PAGE_RW);
    }

    // Fill page diretory higher half
    for(int i = 0; i < PAGE_TABLE_COUNT; i++){
        uint32_t phys_addr = (uint32_t)_page_tables_start + (i*PAGE_SIZE);
        set_page_entry(&_page_directory_start[i+page_directory_start_index], phys_addr, PAGE_PRESENT | PAGE_RW);
    }

    uint32_t virtual_kernel_offset = KERNEL_HIGHER_HALF - (uint32_t)_kernel_start;
    enable_paging((uint32_t)_page_directory_start, virtual_kernel_offset);

    printf("Paging enabled\n");
    paging_enabled = true;
}
