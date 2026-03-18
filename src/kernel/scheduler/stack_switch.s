.code32
.section .text

.globl switch_to_stack
switch_to_stack:
    /* 
       void switch_to_stack(uint32_t* old_esp_ptr, uint32_t new_esp); 
       Stack on entry:
       [esp + 8] : new_esp
       [esp + 4] : old_esp_ptr
       [esp    ] : return address
    */

    /* 1. Save the current stack pointer into the 'old' process struct */
    movl    4(%esp), %eax      /* eax = old_esp_ptr */
    movl    %esp, (%eax)       /* *old_esp_ptr = %esp */

    /* 2. Load the stack pointer from the 'new' process */
    movl    8(%esp), %edx      /* edx = new_esp */
    movl    %edx, %esp         /* SWAP! The stack is now the 'to' stack. */

    /* 3. Return into the 'to' process */
    /* Because we just changed %esp, this 'ret' will pop the return address
       that was saved on the NEW process's stack. */
    ret

.globl switch_to_stack_and_load_cr3
switch_to_stack_and_load_cr3:
    /* [esp+12] : new_cr3, [esp+8] : new_esp, [esp+4] : old_esp_ptr */
    
    /* 1. Save old stack */
    movl    4(%esp), %eax
    movl    %esp, (%eax)

    /* 2. Load new stack */
    movl    8(%esp), %edx
    movl    %edx, %esp

    /* 3. NOW switch the address space (CR3) */
    /* Since we are on the new stack, and the new stack is (hopefully) 
       mapped in the new CR3, this is safe! */
    movl    12(%esp), %ecx
    movl    %ecx, %cr3

    ret

.globl fork_ret
fork_ret:
    /* 
       When we get here, %esp is pointing at the 'gs' member 
       of our interrupt_frame_t. 
    */
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popal       /* Pops edi, esi, ebp, esp (ignored), ebx, edx, ecx, eax */
    addl $8, %esp  /* Skip int_no and err_code */
    iret        /* Jumps to user-space EIP! */