/* interrupts.s — GAS, AT&T syntax, 32-bit */
    .code32
    .text

/* ------------------------------------------------------------------
   Exposed symbols (for C):
     void idt_flush(struct idt_ptr* idtp);
     extern void isr0...isr31;
     extern void irq0...irq15;

   External C handlers (you implement in C):
     void isr_common_handler(void* frame);
     void irq_common_handler(void* frame);

   Assumptions:
     - GDT selectors: 0x08 = kernel code, 0x10 = kernel data
     - PIC remapped so IRQs are 0x20..0x2F
   ------------------------------------------------------------------ */

    .globl idt_flush
idt_flush:
    movl 4(%esp), %eax        /* arg: &idtr {limit, base} */
    lidt (%eax)
    ret

/* ------------- Helpers ------------------------------------------------ */

    .macro PUSH_SEGS
    push %ds
    push %es
    push %fs
    push %gs
    .endm

    .macro POP_SEGS
    pop %gs
    pop %fs
    pop %es
    pop %ds
    .endm

    .macro LOAD_KERNEL_SEGS
    mov $0x10, %ax            /* kernel data selector */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    .endm

/* After pushing int_no and err_code, we jump here.
   Layout on stack *before* pusha:
      [esp] -> err_code
               int_no
   After we enter common stub:
      pusha                     ; push GPRs
      PUSH_SEGS                 ; push old segs
      LOAD_KERNEL_SEGS          ; to safely access kernel data
      push %esp                 ; arg: pointer to full frame (top at saved segs)
      call <C handler>
      add $4, %esp
      POP_SEGS
      popa
      add $8, %esp              ; discard int_no + err_code
      iret
*/

    .globl isr_common_stub
isr_common_stub:
    pusha
    PUSH_SEGS
    LOAD_KERNEL_SEGS
    push %esp
    call isr_common_handler
    add $4, %esp
    POP_SEGS
    popa
    add $8, %esp
    iret

    .globl irq_common_stub
irq_common_stub:
    pusha
    PUSH_SEGS
    LOAD_KERNEL_SEGS
    push %esp
    call irq_common_handler
    add $4, %esp
    POP_SEGS
    popa
    add $8, %esp
    iret

/* ------------- ISR (CPU exceptions 0..31) ----------------------------- */
/* Exceptions that PUSH an error code: 8, 10–14, 17 (and 30 on newer CPUs).
   We’ll handle the classic set: 8, 10, 11, 12, 13, 14, 17. Others get a dummy 0. */

    .macro ISR_NOERR n
    .globl isr\n
isr\n:
    pushl $0                   /* dummy err code */
    pushl $\n                  /* int number */
    jmp isr_common_stub
    .endm

    .macro ISR_ERR n
    .globl isr\n
isr\n:
    /* CPU already pushed a real err code */
    pushl $\n
    jmp isr_common_stub
    .endm

    ISR_NOERR 0       /* #DE Divide Error */
    ISR_NOERR 1       /* Debug */
    ISR_NOERR 2       /* NMI */
    ISR_NOERR 3       /* Breakpoint */
    ISR_NOERR 4       /* Overflow */
    ISR_NOERR 5       /* BOUND */
    ISR_NOERR 6       /* Invalid Opcode */
    ISR_NOERR 7       /* Device Not Available */
    ISR_ERR   8       /* Double Fault (err code) */
    ISR_NOERR 9       /* Coprocessor Segment Overrun (reserved) */
    ISR_ERR  10       /* Invalid TSS */
    ISR_ERR  11       /* Segment Not Present */
    ISR_ERR  12       /* Stack-Segment Fault */
    ISR_ERR  13       /* General Protection */
    ISR_ERR  14       /* Page Fault */
    ISR_NOERR 15      /* reserved */
    ISR_NOERR 16      /* x87 FP Exception */
    ISR_ERR  17       /* Alignment Check */
    ISR_NOERR 18      /* Machine Check */
    ISR_NOERR 19      /* SIMD FP Exception */
    ISR_NOERR 20      /* Virtualization */
    ISR_NOERR 21      /* Control Protection (if present; treat no-err for now) */
    ISR_NOERR 22
    ISR_NOERR 23
    ISR_NOERR 24
    ISR_NOERR 25
    ISR_NOERR 26
    ISR_NOERR 27
    ISR_NOERR 28
    ISR_NOERR 29
    ISR_NOERR 30      /* (Security Exception on newer CPUs: can have err code) */
    ISR_NOERR 31

/* ------------- IRQ (PIC 0..15 → vectors 0x20..0x2F) ------------------ */

    .macro IRQ n, vec
    .globl irq\n
irq\n:
    pushl $0                   /* dummy err code (IRQs never push one) */
    pushl $\vec                /* vector number after PIC remap */
    jmp irq_common_stub
    .endm

    IRQ 0,  0x20
    IRQ 1,  0x21
    IRQ 2,  0x22
    IRQ 3,  0x23
    IRQ 4,  0x24
    IRQ 5,  0x25
    IRQ 6,  0x26
    IRQ 7,  0x27
    IRQ 8,  0x28
    IRQ 9,  0x29
    IRQ 10, 0x2A
    IRQ 11, 0x2B
    IRQ 12, 0x2C
    IRQ 13, 0x2D
    IRQ 14, 0x2E
    IRQ 15, 0x2F
