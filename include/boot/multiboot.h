#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_MAGIC          0x2BADB002

/* Multiboot info flags */
#define MULTIBOOT_INFO_MEM       0x001
#define MULTIBOOT_INFO_BOOTDEV   0x002
#define MULTIBOOT_INFO_CMDLINE   0x004
#define MULTIBOOT_INFO_MODS      0x008
#define MULTIBOOT_INFO_AOUT_SYMS 0x010
#define MULTIBOOT_INFO_ELF_SHDR  0x020
#define MULTIBOOT_INFO_MEM_MAP   0x040

/* Memory info (lower/upper) */
typedef struct multiboot_info {
    uint32_t flags;

    /* Valid if flags[0] set */
    uint32_t mem_lower;
    uint32_t mem_upper;

    /* Valid if flags[1] set */
    uint32_t boot_device;

    /* Valid if flags[2] set */
    uint32_t cmdline;

    /* Valid if flags[3] set */
    uint32_t mods_count;
    uint32_t mods_addr;

    /* a.out or ELF info (depends on flags) */
    uint32_t syms[4];

    /* Valid if flags[6] set */
    uint32_t mmap_length;
    uint32_t mmap_addr;

    /* ... other optional fields omitted for now */
} __attribute__((packed)) multiboot_info_t;

/* Memory map entry (from mmap_addr) */
typedef struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif /* MULTIBOOT_H */
