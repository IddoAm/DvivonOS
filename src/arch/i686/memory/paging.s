/* Paging setup and enabling paging on i686 architecture */
    .code32
    .globl enable_paging
enable_paging:
    /* void enable_paging(uint32_t page_directory_phys, uint32_t virtual_kernel_offset); */
    /* %eax = page_directory_phys, %ebx = virtual_kernel_offset */

    /* Load page directory into CR3 */
    mov     %eax, %cr3

    /* Enable paging: set PG bit (bit 31) in CR0 */
    mov     %cr0, %eax
    or      $0x80000000, %eax    /* set PG bit */
    mov     %eax, %cr0

    ret