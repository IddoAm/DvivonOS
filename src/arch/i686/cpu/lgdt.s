
    .code32
    .globl gdt_load
gdt_load:
    /* void gdt_load(struct gdt_ptr* gp); */
    mov     4(%esp), %eax      /* eax = &gdt_ptr */
    lgdt    (%eax)             /* load GDTR from *gp */

    /* Far jump to reload CS with selector 0x08 (GDT index 1, RPL 0) */
    ljmp    $0x08, $flush_cs

    .globl tss_load
tss_load:
    /* void tss_load(uint16_t sel); */
    mov     4(%esp), %ax      /* ax = sel */
    ltr     %ax                 /* load TR with sel */

flush_cs:
    /* Load data segment registers with selector 0x10 (GDT index 2) */
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss
    ret
