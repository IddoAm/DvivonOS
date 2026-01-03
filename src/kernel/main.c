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

void user_space_loop() {
    while (true) {

        printf("Hello from User Space");
        for (volatile int i = 0; i < 10000000; i++);
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

    multiboot_mmap_entry_t* mmap = loader_get_memory_map();
    uint32_t mmap_end = loader_get_memory_map_length() + (uintptr_t)mmap;
    while ((uintptr_t)mmap < (mmap_end)) {
        printf("Region: base=0x%x%x, len=0x%x%x, type=%d\n", (uint32_t)(mmap->addr >> 32),
               (uint32_t)mmap->addr, (uint32_t)(mmap->len >> 32), (uint32_t)mmap->len, mmap->type);

        mmap = (multiboot_mmap_entry_t*)((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
    }

    gdt_init();
    idt_init();

    pmm_init(loader_get_memory_map(), loader_get_memory_map_length());
	vmm_init();
	heap_init_kernel(0xC0400000, 16);

    printf("syscall: running write test via int 0x80\n");
    const char test_msg[] = "syscall test: hello from syscall_write\n";
    int r = do_syscall_write(1, test_msg, sizeof(test_msg) - 1);
    printf("syscall returned %d\n", r);

    // Start timer before starting scheduler so IRQ0 fires
    clock_init(100); // 100 Hz
   // scheduler_start();
     
 }
