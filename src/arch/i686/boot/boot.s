.code32

.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot.data, "aw"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.set HIGHER_HALF_BASE, 0xC0000000

.extern _page_directory_start
.extern _page_tables_start

.extern _kernel_start
.extern _kernel_end

.section .multiboot.text, "a"
.global _start
.type _start, @function
_start:
    # Save multiboot registers (caller of kernel_main expects magic in eax, info in ebx)
    push %ebx
    push %eax

    # Set up a physical stack before enabling paging:
    movl $stack_top, %ebp
    subl $HIGHER_HALF_BASE, %ebp    # ebp = stack_top_phys
    movl %ebp, %esp

    # Compute physical addresses at runtime:
    movl $_page_tables_start, %ebx
    subl $HIGHER_HALF_BASE, %ebx    # ebx = page_tables_phys

    movl $_page_directory_start, %edx
    subl $HIGHER_HALF_BASE, %edx    # edx = page_directory_phys

    movl $_kernel_start, %esi

    movl $_kernel_end, %edi
    subl $HIGHER_HALF_BASE, %edi    # edi = kernel_end_phys

    # Prepare pte pointer and loop variables
    movl %ebx, %ebp          # ebp = pte_ptr (page_tables_phys)
    xorl %eax, %eax          # eax = current phys addr (starts 0)
    movl $1023, %ecx         # number of pages to scan

mapping_loop:
    cmpl %esi, %eax
    jb skip_page
    cmpl %edi, %eax
    jae end_mapping

    # Map page: write PTE = phys | 3
    movl %eax, (%ebp)        # write physical frame
    orl $0x003, (%ebp)       # set present|rw in-place (no reg clobber)

skip_page:
    addl $4096, %eax
    addl $4, %ebp
    loop mapping_loop

end_mapping:
    # reload page_directory_phys (it may have been clobbered)
    movl $_page_directory_start, %edx
    subl $HIGHER_HALF_BASE, %edx    # edx = page_directory_phys

    # Set VGA PTE at last entry of first page table
    movl %ebx, %ecx
    addl $((1023) * 4), %ecx
    movl $0x000B8000, %eax    # use EAX as temp so %edx stays page_directory_phys
    orl $0x003, %eax
    movl %eax, (%ecx)

    # Identity-map first 4MB: PDE0 = page_tables_phys | 3
    movl %ebx, %ecx
    orl $0x003, %ecx
    movl %ecx, (%edx)        # write to page_directory_phys (in %edx)
    # Above line: %edx currently holds page_directory_phys from earlier

    # Map kernel PDEs (768..1023 -> 256 PDEs)
    movl %edx, %ebp         # ebp = page_directory_phys
    addl $(768*4), %ebp     # ebp points at PDE[768]
    movl %ebx, %ecx         # ecx = page_tables_phys (first kernel table)
    movl $256, %esi         # esi = count

map_kernel_pdes:
    movl %ecx, (%ebp)
    orl $0x003, (%ebp)
    addl $4096, %ecx        # next physical page table
    addl $4, %ebp           # next PDE
    decl %esi
    jnz map_kernel_pdes

    # Load CR3 and enable paging
    movl %edx, %eax         # page_directory_phys -> eax
    movl %eax, %cr3
    movl %cr0, %eax
    orl $0x80000000, %eax    # set PG bit
    movl %eax, %cr0

    # Jump to higher-half entry (compute virtual address)
    movl $_higher_half_entry, %eax
    addl $HIGHER_HALF_BASE, %eax
    jmp *%eax

_higher_half_entry:
    # Restore multiboot args into registers and call kernel_main:
    pop %eax   #_restore magic (was pushed first)
    pop %ebx   #_restore mb info
    push %ebx
    push %eax
    call kernel_main

.hang:
    cli
1:  hlt
    jmp 1b

.size _start, . - _start
