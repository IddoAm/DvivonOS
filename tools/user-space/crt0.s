# crt0.s - The very first code executed in userspace

.section .text
.globl _start

_start:
    xorl %ebp, %ebp

    # Stack layout set up by the kernel in process_init():
    #   [esp+0]          = argc  (int)
    #   [esp+4]          = argv[0] pointer
    #   ...
    #   [esp+4*argc]     = argv[argc-1] pointer
    #   [esp+4*(argc+1)] = NULL  (end of argv[])
    #   ... argv string data ...

    popl   %eax           # eax = argc  (esp now points to argv[0])
    movl   %esp, %ebx     # ebx = argv  (pointer to argv[0] on stack)

    # Call main(argc, argv)
    pushl  %ebx           # arg2: argv
    pushl  %eax           # arg1: argc
    call   main
    addl   $8, %esp       # clean up 2 pushed args

    pushl  %eax
    call   _exit

    # If _exit fails to terminate, hang the CPU
.halt:
    hlt
    jmp .halt
