# enable_paging(uint32_t cr3_phys, uint32_t virt_offset)
# cdecl stack:
#   [esp+4]  = RET (identity address, e.g. 0x0020xxxx)
#   [esp+8]  = cr3_phys
#   [esp+12] = virt_offset (== KERNEL_HIGHER_HALF, e.g. 0xC0000000)

    .section .text
    .globl enable_paging
    .type  enable_paging, @function
enable_paging:
    pushl   %ebp
    movl    %esp, %ebp

    # Load CR3 (assumed aligned + valid)
    movl    8(%ebp), %eax
    movl    %eax, %cr3

    # Enable paging (set CR0.PG)
    movl    %cr0, %eax
    orl     $0x80000000, %eax
    movl    %eax, %cr0
    jmp     1f
1:

    # Always patch RET with virt_offset 
    movl    4(%ebp), %eax
    movl    12(%ebp), %edx
    addl    %edx, %eax
    movl    %eax, 4(%ebp)

    # Move Stack to higher-half
    addl    %edx, %esp
    addl    %edx, %ebp

    popl    %ebp
    ret


# reload_cr3(uint32_t cr3_phys)
# cdecl stack:
#   [esp+4] = cr3_phys

    .section .text
    .globl reload_cr3
    .type  reload_cr3, @function
reload_cr3:
    movl 4(%esp), %eax
    movl %eax, %cr3
    ret
