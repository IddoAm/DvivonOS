ENTRY(_start)
SECTIONS
{
    /* Pick a virtual address that your usermode handles */
    . = 0x400000; 

    .text : { *(.text) }
    .data : { *(.data) }
    .bss  : { *(.bss) }
}