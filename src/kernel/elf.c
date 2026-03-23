#include <kernel/elf.h>
#include <kernel/vmm.h>
#include <kernel/scheduler/scheduler.h>
#include <lib/string.h>

#define PT_LOAD 1

process_t* load_elf(void* buffer, uint32_t size,
                    int argc, const char** argv, const char* cwd) {
    elf_header_t* header = (elf_header_t*)buffer;

    // 1. Validation
    if (header->ident[0] != 0x7F || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F') {
        return NULL;
    }

    // 2. Create the process structure and address space
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) return NULL;
    process_init(proc, (void*)header->entry, argc, argv, cwd ? cwd : "/");

    // 3. Switch to the new process address space to perform the load
    uint32_t old_cr3 = vmm_read_cr3();
    vmm_switch_address_space(proc->pd_phys);

    // 4. Parse Program Headers
    elf_program_header_t* ph = (elf_program_header_t*)((uint8_t*)buffer + header->ph_offset);

    for (uint32_t i = 0; i < header->ph_count; i++) {
        if (ph[i].type == PT_LOAD) {
            // 5. Allocate memory for this segment
            for (uint32_t vaddr = ph[i].vaddr; vaddr < ph[i].vaddr + ph[i].memsz; vaddr += PAGE_SIZE) {
                uint32_t page_aligned_vaddr = vaddr & 0xFFFFF000;
                vmm_alloc_user_page_at(page_aligned_vaddr);
            }

            // 6. Copy the data from the ELF buffer to the virtual address
            memcpy((void*)ph[i].vaddr, (uint8_t*)buffer + ph[i].offset, ph[i].filesz);

            // 7. Handle BSS: Zero out the remainder of memsz
            if (ph[i].memsz > ph[i].filesz) {
                memset((uint8_t*)ph[i].vaddr + ph[i].filesz, 0, ph[i].memsz - ph[i].filesz);
            }
        }
    }

    // 8. Restore the original address space
    vmm_switch_address_space(old_cr3);

    return proc;
}