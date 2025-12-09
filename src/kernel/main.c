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

void shell_loop2() {
    printf("tests");
    while (true) {

        printf("#");
        for (volatile int i = 0; i < 1000000; i++)
            ;
    }
}

void shell_loop1() {
    printf("tests");
    while (true) {

        printf("-");
        for (volatile int i = 0; i < 1000000; i++)
            ;
    }
}

void shell_loop3() {
    printf("tests");
    while (true) {

        printf("+");
        for (volatile int i = 0; i < 1000000; i++)
            ;
    }
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

    uint32_t* allocation = (uint32_t*)kmalloc(sizeof(uint32_t));
    *allocation = 5;
    printf("%d, %x\n", *allocation, allocation);
    uint32_t* allocation2 = (uint32_t*)kmalloc(sizeof(uint32_t));
    *allocation2 = 10;
    printf("%d, %x\n", *allocation2, allocation2);
    printf("%d, %x\n", *allocation, allocation);
    printf("freeing second allocation\n");

    kfree((void*)allocation2);

    allocation2 = (uint32_t*)kmalloc(sizeof(uint32_t));
    *allocation2 = 15;
    printf("%d, %x\n", *allocation2, allocation2);
    printf("%d, %x\n", *allocation, allocation);
    kfree((void*)allocation);
    kfree((void*)allocation2);

    process_t proc1;
    process_init(&proc1, shell_loop1);
    scheduler_init();

    //isr_register_handler(irq_to_vector(1), keyboard_callback);
   // pic_clear_mask(1);

    clock_init(100); // 100 Hz
    
}
