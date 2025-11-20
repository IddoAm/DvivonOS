#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <drivers/keyboard.h>
#include <drivers/vga.h>
#include <lib/stdio.h>

#include <boot/loader.h>

#include <kernel/heap-allocator.h>
#include <kernel/pmm.h>
#include <kernel/scheduler/scheduler.h>
#include <kernel/time/time.h>
#include <kernel/vmm.h>

#include <arch/i686/pic.h>
#include <prog/terminal.h>

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
    loader_init(magic, virt_addr, phys_addr);
    gdt_init();
    idt_init();
    terminal_initialize();

    isr_register_handler(irq_to_vector(1), keyboard_callback);

    printf("%omaping all the memory regions.\n  %o1. %ofor usable, \n  %o2. %ofor reserved\n",
           STD_COLOR_LIGHT_BLUE, 1, STD_COLOR_LIGHT_BLUE, 2, STD_COLOR_LIGHT_BLUE);

    multiboot_mmap_entry_t* mmap = loader_get_memory_map();
    uint32_t mmap_end = loader_get_memory_map_length() + (uintptr_t)mmap;
    while ((uintptr_t)mmap < (mmap_end)) {
        printf("Region: base=0x%x%x, len=0x%x%x, type=%o%d\n", (uint32_t)(mmap->addr >> 32),
               (uint32_t)mmap->addr, (uint32_t)(mmap->len >> 32), (uint32_t)mmap->len, mmap->type,
               mmap->type);

        mmap = (multiboot_mmap_entry_t*)((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
    }

    pmm_init(loader_get_memory_map(), loader_get_memory_map_length());

    printf("%oTesting kernel heap allocator:\n", STD_COLOR_LIGHT_BLUE);

    uint32_t* allocation = (uint32_t*)kmalloc(sizeof(uint32_t));
    *allocation = 5;
    printf("Storing %d, in %x\n", *allocation, allocation);
    uint32_t* allocation2 = (uint32_t*)kmalloc(sizeof(uint32_t));
    *allocation2 = 10;
    printf("Storing %d, in %x\n", *allocation2, allocation2);
    printf("Check that %d is still at %x\n", *allocation, allocation);
    printf("freeing second allocation\n");

    kfree((uintptr_t)allocation2);

    allocation2 = (uint32_t*)kmalloc(sizeof(uint32_t));
    *allocation2 = 15;
    printf("Storing %d, in %x\n", *allocation2, allocation2);
    printf("Check that %d is still at %x\n", *allocation, allocation);
    kfree((uintptr_t)allocation);
    kfree((uintptr_t)allocation2);

    pic_clear_mask(1);

    clock_init(100); // 100 Hz

    // print all the colors
    printf("%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo\n", STD_COLOR_BLACK, STD_COLOR_BLUE,
           STD_COLOR_GREEN, STD_COLOR_CYAN, STD_COLOR_RED, STD_COLOR_MAGENTA, STD_COLOR_BROWN,
           STD_COLOR_LIGHT_GREY, STD_COLOR_DARK_GREY, STD_COLOR_LIGHT_BLUE, STD_COLOR_LIGHT_GREEN,
           STD_COLOR_LIGHT_CYAN, STD_COLOR_LIGHT_RED, STD_COLOR_LIGHT_MAGENTA,
           STD_COLOR_LIGHT_BROWN, STD_COLOR_WHITE);
    // Main Loop
    key_event event;
    while (true) {
        if (keyboard_read(&event)) {
            terminal_handle_keypress(event);
        }
        __asm__ volatile("hlt");
    }
}
