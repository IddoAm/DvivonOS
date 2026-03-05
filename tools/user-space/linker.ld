ENTRY(_start)

SECTIONS
{
    /* Common userspace start address is 1MB or 128MB */
    . = 0x100000; 

    .text : { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { 
        *(.bss) 
        *(COMMON) 
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}