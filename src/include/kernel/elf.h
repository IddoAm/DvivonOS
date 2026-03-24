#ifndef ELF_LOADING_H
#define ELF_LOADING_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/scheduler/scheduler.h>

typedef struct {
    uint8_t  ident[16];   // Magic number and other info
    uint16_t type;        // Object file type
    uint16_t machine;     // Architecture (0x03 for x86)
    uint32_t version;     // Object file version
    uint32_t entry;       // Entry point virtual address
    uint32_t ph_offset;   // Program header table file offset
    uint32_t sh_offset;   // Section header table file offset
    uint32_t flags;       // Processor-specific flags
    uint16_t eh_size;     // ELF header size in bytes
    uint16_t ph_size;     // Program header table entry size
    uint16_t ph_count;    // Program header table entry count
    uint16_t sh_size;     // Section header table entry size
    uint16_t sh_count;    // Section header table entry count
    uint16_t sh_str_index;// Section header string table index
} __attribute__((packed)) elf_header_t;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} elf_program_header_t;

// Returns the newly created process_t* on success, NULL on failure.
process_t* load_elf(void* buffer, uint32_t size,
                    int argc, const char** argv, const char* cwd);

#endif // ELF_LOADING_H