#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <drivers/keyboard.h>
#include <drivers/vga.h>
#include <lib/stdio.h>

#include <boot/loader.h>

#include <kernel/heap_allocator.h>
#include <kernel/pmm.h>
#include <kernel/scheduler/scheduler.h>
#include <kernel/time/time.h>
#include <kernel/vmm.h>

#include <arch/i686/pic.h>
#include <kernel/syscall.h>

#include <lib/string.h>

void user_space_loop() {
    while (true) {

        printf("Hello from User Space");
        for (volatile int i = 0; i < 10000000; i++);
    }
}

void dump_page_directory() {
    uint32_t* pd = (uint32_t*)0xFFFFF000;
    
    printf("--- Page Directory Dump ---\n");
    for (int i = 0; i < 768; i++) {
        // Only print entries that are "Present" (bit 0 is set)
        if (pd[i] & 1) {
            printf("PDE [%d]: 0x%x + +", i, pd[i]);
        }
    }
}

static int do_syscall_write(int fd, const char* buf, size_t len) {
    uint32_t ret;
    __asm__ volatile(
        "int $0x67"
        : "=a"(ret)
        : "a"((uint32_t)SYSCALL_WRITE), "b"((uint32_t)fd), "c"((uint32_t)buf), "d"((uint32_t)len)
        : "memory"
    );
    return (int)ret;
}

void kernel_main(uint32_t magic, uint32_t virt_addr, uint32_t phys_addr) {
    // todo: move this to terminal file
    stdio_interface_t vga_interface = {
        .init = vga_initialize, .clear = vga_clear, .putc = vga_putchar, .puts = vga_writestring};
    stdio_set_interface(&vga_interface);
    stdio_init();
    printf("Welcome to Iddo's and hillel's amazing OS\n");

    loader_init(magic, virt_addr, phys_addr);

    gdt_init();
    idt_init();

    pmm_init(loader_get_memory_map(), loader_get_memory_map_length());

    syscall_init();

    /*
    printf("syscall: running write test via int 0x67\n");
    const char test_msg[] = "syscall test: hello from syscall_write\n";
    int r = do_syscall_write(1, test_msg, sizeof(test_msg) - 1);
    printf("syscall returned %d\n", r);
    */
    printf("allocating some memory\n");
    uint32_t* value = (uint32_t*)kmalloc(sizeof(uint32_t));
    *value = 42;
    printf("Allocated value: %d at %x\n", *value, value);
    kfree((uintptr_t)value);
    printf("Freed allocated memory\n");

    /* create a user process and load a tiny i686 user program */
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) {
        printf("failed to alloc process struct\n");
    } else {
        memset(proc, 0, sizeof(*proc));
        printf("la");
        // set initial EIP to PROCESS_HEAP_START (we'll put the program there)
        process_init(proc, (void*)PROCESS_HEAP_START);
        printf("la");
        // Example i686 user loop: syscall_write(1, msg, len) using int 0x67 then loop
        const uint8_t user_code[] = {
            0xB8, 0x01,0x00,0x00,0x00,    // mov eax, 1          ; SYSCALL_WRITE (adjust if different)
            0xBB, 0x01,0x00,0x00,0x00,    // mov ebx, 1          ; fd = 1
            0xB9, 0x00,0x00,0x00,0x00,    // mov ecx, <msg_addr> ; patched below
            0xBA, 0x0F,0x00,0x00,0x00,    // mov edx, 15         ; len of message
            0xCD, 0x67,                   // int 0x67            ; syscall interrupt
            0xEB, 0xF0                    // jmp short -16       ; loop forever
        };
        const char msg[] = "Hello from i686\n"; // 15 bytes (without terminating 0)
        const size_t code_len = sizeof(user_code);
        const size_t msg_len = sizeof(msg) - 1;
        const uint32_t code_vaddr = PROCESS_HEAP_START;
        const uint32_t msg_vaddr = code_vaddr + (uint32_t)code_len;
        printf("Check again\n");
        // patch ecx immediate (little endian)
        uint8_t code_patched[sizeof(user_code)];
        memcpy(code_patched, user_code, sizeof(user_code));
        *(uint32_t*)&code_patched[11] = msg_vaddr;
        printf("Check again\n");
        // load code + message into the process address space
        if (!process_load_user_memory(proc, code_vaddr, code_patched, code_len)) {
            printf("failed to load user code\n");
        }
        printf("Check again\n");
        if (!process_load_user_memory(proc, msg_vaddr, msg, msg_len)) {
            printf("failed to load user message\n");
        }
        printf("Check again\n");

        // add to scheduler
        scheduler_add_process(proc);
    }
    printf("LALA\n");
    // Start timer before starting scheduler so IRQ0 fires
    clock_init(100); // 100 Hz
    scheduler_start();

}
