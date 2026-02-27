#include <kernel/elf.h>
#include <kernel/vmm.h>
#include <lib/stdio.h>


int load_elf(void* buffer, uint32_t size) {
    elf_header_t* header = (elf_header_t*)buffer;
    if (header->ident[0] != 0x7F || header->ident[1] != 'E' || 
        header->ident[2] != 'L' || header->ident[3] != 'F') {
        return -1; // Not a valid ELF
    }
    return 0;
}