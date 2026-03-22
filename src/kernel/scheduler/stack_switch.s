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

    /* 1. Save callee-saved registers on the current stack */
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    /* 2. Save the current stack pointer into the 'old' process struct */
    movl    20(%esp), %eax     /* eax = old_esp_ptr  (4 pushes shifted args by 16) */
    movl    %esp, (%eax)       /* *old_esp_ptr = %esp */

    /* 3. Load the stack pointer from the 'new' process */
    movl    24(%esp), %edx     /* edx = new_esp */
    movl    %edx, %esp         /* SWAP! The stack is now the 'to' stack. */

    /* 4. Restore callee-saved registers from the new stack */
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    /* 5. Return into the 'to' process */
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
