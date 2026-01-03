#include <kernel/syscall.h>
#include <arch/i686/idt.h>
#include <lib/stdio.h>

void syscall_handler(interrupt_frame_t* frame) {
    uint32_t num = frame->eax;
    int ret = ENOSYS;
    
    switch(num) {
        case SYSCALL_EXIT:
            printf("syscall: exit(%d)\n", frame->ebx);
            while(1) __asm__ volatile("hlt");
            break;
            
        case SYSCALL_WRITE: {
            int fd = frame->ebx;
            const char* buf = (const char*)frame->ecx;
            size_t len = frame->edx;
            
            if (fd == 1 || fd == 2) {
                for (size_t i = 0; i < len; i++) {
                    putc(buf[i]);
                }
                ret = (int)len;
            } else {
                ret = EBADF;
            }
            break;
        }
        
        default:
            printf("syscall: unknown syscall %d\n", num);
            ret = ENOSYS;
            break;
    }
    
    frame->eax = (uint32_t)ret;
}