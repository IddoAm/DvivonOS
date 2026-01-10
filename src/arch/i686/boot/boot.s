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

.align 4
multiboot_copy:
.skip 4096
multiboot_copy_end:


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
    # Set up a physical stack before enabling paging:
    movl $stack_top, %ebp
    subl $HIGHER_HALF_BASE, %ebp    # ebp = stack_top_phys
    movl %ebp, %esp

	# Push original multiboot info address
	push %ebx

	# Copy the multiboot info (up to 4 KB)
    movl %ebx, %esi              # ESI = source (physical)
    movl $multiboot_copy, %edi   # EDI = destination (virtual)
	subl $HIGHER_HALF_BASE, %edi 
    movl $1024, %ecx             # 4KB / 4 = 1024 dwords
    rep movsl                    # copy 4KB of data
	movl $multiboot_copy, %ebx

	push %ebx
    push %eax


    # Compute physical addresses at runtime:
    movl $_page_tables_start, %ebx
    subl $HIGHER_HALF_BASE, %ebx    # ebx = page_tables_phys

    movl $_page_directory_start, %edx
    subl $HIGHER_HALF_BASE, %edx    # edx = page_directory_phys

    # Zero out the entire page directory
    movl %edx, %edi                  # edi = page_directory_phys
    xorl %eax, %eax                  # zero
    movl $1024, %ecx                 # 1024 entries (4096 bytes / 4)
    rep stosl                        # zero all PDEs

    # Also zero out all page tables
    movl %ebx, %edi                  # edi = page_tables_phys
    xorl %eax, %eax                  # zero
    movl $(255 * 1024), %ecx         # 255 tables * 1024 entries
    rep stosl                        # zero all PTEs

    # Reload registers after using them for zeroing
    movl $_page_tables_start, %ebx
    subl $HIGHER_HALF_BASE, %ebx    # ebx = page_tables_phys

    movl $_page_directory_start, %edx
    subl $HIGHER_HALF_BASE, %edx    # edx = page_directory_phys

    movl $_kernel_start, %esi
    # _kernel_start is already physical no need to subtract

    movl $_kernel_end, %edi
    subl $HIGHER_HALF_BASE, %edi    # edi = kernel_end_phys

    # Prepare pte pointer and loop variables for FIRST page table only
    movl %ebx, %ebp          # ebp = pte_ptr (page_tables_phys)
    xorl %eax, %eax          # eax = current phys addr (starts 0)
    movl $1024, %ecx         # number of pages in first table

mapping_loop:
    # Check if current page is in kernel range
    cmpl %esi, %eax
    jb skip_page
    cmpl %edi, %eax
    jae skip_page

    # Map page: write PTE = phys | 3
    pushl %edx               # Save edx (page_directory_phys)
    movl %eax, %edx
    orl $0x103, %edx         # Present + RW + Global
    movl %edx, (%ebp)
    popl %edx                # Restore edx

skip_page:
    addl $4096, %eax         # Next physical page
    addl $4, %ebp            # Next PTE
    loop mapping_loop

end_mapping:
    # edx still contains page_directory_phys (we preserved it in the loop)

    # Map VGA to 0xC00B8000 (PTE[184])
    movl %ebx, %ecx
    addl $(184 * 4), %ecx      # PTE[184]
    movl $0x000B8000, %eax
    orl $0x103, %eax           # Present + RW + Global
    movl %eax, (%ecx)

    # Identity-map first 4MB: PDE0 = page_tables_phys | 3
    movl %ebx, %ecx
    orl $0x003, %ecx
    movl %ecx, (%edx)        # Write to page_directory[0]

    # Map kernel PDEs (768..1023 -> 255 PDEs)
    movl %edx, %ebp          # ebp = page_directory_phys
    addl $(768*4), %ebp      # ebp points at PDE[768]
    movl %ebx, %ecx          # ecx = page_tables_phys (first kernel table)
    movl $255, %esi          # esi = count

map_kernel_pdes:
    movl %ecx, %eax          # move PT address into eax
    orl  $0x003, %eax        # add flags (Present + RW)
    movl %eax, (%ebp)        # store to PDE
    addl $4096, %ecx         # next page table
    addl $4, %ebp            # next PDE
    decl %esi
    jnz map_kernel_pdes

map_self_refrence_pde:
    # Set up self-referencing PDE at index 1023
    movl %edx, %eax          # page_directory_phys
    orl $0x003, %eax         # Present + RW
    movl %eax, 4092(%edx)    # PDE[1023] = page_directory_phys | 3

finalize_paging:
    # Enable Global Pages (PGE bit in CR4)
    movl %cr4, %eax
    orl $0x80, %eax          # Set bit 7 (PGE)
    movl %eax, %cr4

    # Load CR3 and enable paging
    movl %edx, %eax          # page_directory_phys -> eax
    movl %eax, %cr3
    movl %cr0, %eax
    orl $0x80000000, %eax    # set PG bit
    movl %eax, %cr0

    # Jump to higher-half entry (it's already a virtual address!)
    movl $_higher_half_entry, %eax
    #addl $HIGHER_HALF_BASE, %eax
    jmp *%eax

.size _start, . - _start
.section .text
_higher_half_entry:
	pop %eax   # magic
    pop %ebx   # restore mb info virtual
	pop %ecx   # restore mb info physical

    movl $stack_top, %esp

    # Unmap the identity mapping (PDE[0])
    movl $0xFFFFF000, %edx
    movl $0, (%edx)          # Clear PDE[0]
    
    # Flush TLB by reloading CR3
    movl %cr3, %edx
    movl %edx, %cr3
    # Restore multiboot args into registers and call kernel_main:
	push %ecx
    push %ebx
    push %eax
    call kernel_main

.hang:
    cli
1:  hlt
    jmp 1b