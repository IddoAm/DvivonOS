ENTRY (_start)

KERNEL_VIRT = 0xC0000000;
KERNEL_PHYS = 0x00200000;  
PAGE = 4096;
VMA_BASE = KERNEL_VIRT + KERNEL_PHYS;

SECTIONS
{
    /* Put multiboot header at the start of the file image (within first 8KiB). */
    . = KERNEL_PHYS;
	/* The kernel will live at 3GB + 1MB in the virtual address space, */
	/* which will be mapped to 1MB in the physical address space. */
	/* Note that we page-align the sections. */

	_kernel_start = .;
    .multiboot.data : {
        *(.multiboot.data)
    }

    .multiboot.text : {
        *(.multiboot.text)
    }

    /* Switch to the kernel virtual base (3GB + 1MiB) for VMA placement. */
    . += KERNEL_VIRT;
    /*_kernel_start = .;*/

    /* text/rodata/data/bss: VMA at high half, LMA mapped to KERNEL_PHYS region */
    .text : AT(KERNEL_PHYS + (ADDR(.text) - VMA_BASE))
    {
        . = ALIGN(PAGE);
        *(.text)
    }

    .rodata : AT(KERNEL_PHYS + (ADDR(.rodata) - VMA_BASE))
    {
        . = ALIGN(PAGE);
        *(.rodata)
    }

    .data : AT(KERNEL_PHYS + (ADDR(.data) - VMA_BASE))
    {
        . = ALIGN(PAGE);
        *(.data)
    }

    .bss (NOLOAD) : AT(KERNEL_PHYS + (ADDR(.bss) - VMA_BASE))
    {
        . = ALIGN(PAGE);
        *(COMMON)
        *(.bss)
        *(.bootstrap_stack)
    }

    /* keep your personal reserved sections */
    .isr_table (NOLOAD) : AT(KERNEL_PHYS + (ADDR(.isr_table) - VMA_BASE))
    {
        . = ALIGN(PAGE);
        isr_table_start = .;
        . += PAGE;
        isr_table_end = .;
    }

    .pmm_bitmap (NOLOAD) : AT(KERNEL_PHYS + (ADDR(.pmm_bitmap) - VMA_BASE))
    {
        . = ALIGN(PAGE);
        _pmm_bitmap_start = .;
        . += 1048576 / 8;    /* bytes reserved for bitmap example */
        _pmm_bitmap_end = .;
    }

    .page_directory (NOLOAD) : AT(KERNEL_PHYS + (ADDR(.page_directory) - VMA_BASE))
    {
        . = ALIGN(PAGE);
        _page_directory_start = .;
        . += PAGE;
        _page_directory_end = .;
    }

    .page_tables (NOLOAD) : AT(KERNEL_PHYS + (ADDR(.page_tables) - VMA_BASE))
    {
        . = ALIGN(PAGE);
        _page_tables_start = .;
        . += 255 * PAGE;
        _page_tables_end = .;
    }

    /* Kernel scratch page for syscall copy-in (one page) */
    .scratch (NOLOAD) : AT(KERNEL_PHYS + (ADDR(.scratch) - VMA_BASE))
    {
        . = ALIGN(PAGE);
        __scratch_start = .;
        . += PAGE;
        __scratch_end = .;
    }

    _kernel_end = .;
}