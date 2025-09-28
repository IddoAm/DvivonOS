ENTRY(_start)

SECTIONS
{
    /* Kernel start at 2 MB */
    . = 2M;
    _kernel_start = .;

    /* Multiboot header + .text */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* Read-only data */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Initialized data */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* Uninitialized data (stack + globals) */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    .isr_table (NOLOAD) : ALIGN(4K)
    {
        isr_table_start = .;
        . += 4k; /* 1 page for isr table: 256 ints, 4 max handlers per int */
        isr_table_end = .;
    }

    .pmm_bitmap (NOLOAD) : ALIGN(4K)
    {
        _pmm_bitmap_start = .;
        . += 1048576 / 8;   /* 1048576 is the max pages in 32 bit */
        _pmm_bitmap_end = .;
    }

    .page_directory (NOLOAD) : ALIGN(4K)
    {
        _page_directory_start = .;
        . += 4k;            /* 1 page for page directory */
        _page_directory_end = .;
    }

    .page_tables (NOLOAD) : ALIGN(4K)
    {
        _page_tables_start = .;
        . += 256 * 4k; /* 256 is the amount of page tables in 1 GB of higher half kernel memory */
        _page_tables_end = .;
    }

    _kernel_end = .;
}
