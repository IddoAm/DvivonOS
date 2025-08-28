#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_MAGIC          0x2BADB002

#define MULTIBOOT_INFO_MEM       0x001
#define MULTIBOOT_INFO_BOOTDEV   0x002
#define MULTIBOOT_INFO_CMDLINE   0x004
#define MULTIBOOT_INFO_MODS      0x008
#define MULTIBOOT_INFO_AOUT_SYMS 0x010
#define MULTIBOOT_INFO_ELF_SHDR  0x020
#define MULTIBOOT_INFO_MEM_MAP   0x040

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed)) multiboot_info_t;

typedef struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

typedef enum {
    MULTIBOOT_MEMORY_AVAILABLE = 1,
    MULTIBOOT_MEMORY_RESERVED  = 2,
    MULTIBOOT_MEMORY_ACPI_RECLAIMABLE = 3,
    MULTIBOOT_MEMORY_NVS       = 4,
    MULTIBOOT_MEMORY_BADRAM    = 5
} multiboot_memory_type_t;

#endif /* MULTIBOOT_H */
