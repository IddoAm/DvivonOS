#ifndef VMM_H
#define VMM_H

#include <stdint.h>

extern uint32_t _page_directory_start[];
extern uint32_t _page_tables_start[];

void vmm_init();

uintptr_t vmm_alloc_page();
void vmm_free_page(const uintptr_t addr);

#endif