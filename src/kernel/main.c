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
#include <prog/shell-loops.h>
#include <prog/shell.h>
#include <kernel/syscall.h>
#include <lib/string.h>

#include <kernel/elf.h>

#include <fs/vfs/filesystem.h>
#include <fs/vfs/mount.h>
#include <drivers/disk/ata.h>

#include <lib/fs.h>

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


void init_keyboard() {
    isr_register_handler(irq_to_vector(1), keyboard_callback);
}

void start_the_fs(void){
    ata_init();
    filesystem_init();
    printf("[fs] Filesystem initialized\n");
    if (mount_init() != 0)
        printf("[fs] No filesystem available (no IDE disk or no ext2)\n");
}

void add_elf_proc(char* path)
{
    int size = fs_file_size(path);
    printf("File size: %d\n", size);
    void* buffer = (void*)kmalloc(size);
    fs_read_file(path, buffer, size);
    if (load_elf(buffer, size) != 0) {
        printf("%oELF verification failed!\n", STD_COLOR_LIGHT_RED);
        return;
    }
    printf("%oELF verification succeeded!\n", STD_COLOR_LIGHT_GREEN);
    kfree((uintptr_t)buffer);
}

void print_memory_regions() {
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
}

void test_elf_loading() {
    printf("%oTesting ELF loading...\n", STD_COLOR_LIGHT_BLUE);
    add_elf_proc("/public-bin/test1");
    add_elf_proc("/public-bin/test2");
}

void init_memory_management() {
    pmm_init(loader_get_memory_map(), loader_get_memory_map_length());
    syscall_init();
}

void test_heap_allocator() {
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
}

void init_hardware() {
    pic_clear_mask(1);
    clock_init(100); // 100 Hz
}

void print_all_colors_test() {
    printf("%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo%oo\n", STD_COLOR_BLACK, STD_COLOR_BLUE,
           STD_COLOR_GREEN, STD_COLOR_CYAN, STD_COLOR_RED, STD_COLOR_MAGENTA, STD_COLOR_BROWN,
           STD_COLOR_LIGHT_GREY, STD_COLOR_DARK_GREY, STD_COLOR_LIGHT_BLUE, STD_COLOR_LIGHT_GREEN,
           STD_COLOR_LIGHT_CYAN, STD_COLOR_LIGHT_RED, STD_COLOR_LIGHT_MAGENTA,
           STD_COLOR_LIGHT_BROWN, STD_COLOR_WHITE);
}

void _run_tests()
{
    print_memory_regions();
    test_heap_allocator();
    print_all_colors_test();
    test_elf_loading();

}

void main_loop() {
    key_event event;
    while (true) {
        if (keyboard_read(&event)) {
            terminal_handle_keypress(event);
        }
        __asm__ volatile("hlt");
    }
}

// a helper function to initialize kernel subsystems
static void _init(uint32_t magic, uint32_t virt_addr, uint32_t phys_addr)
{
    loader_init(magic, virt_addr, phys_addr);
    gdt_init();
    idt_init();
    terminal_initialize();
    init_keyboard();
    init_memory_management();
    init_hardware();
    start_the_fs();

}

void kernel_main(uint32_t magic, uint32_t virt_addr, uint32_t phys_addr) {

    _init(magic, virt_addr, phys_addr);
    _run_tests();

    //create_and_schedule_user_process();
    scheduler_start();
    
    
    //test_filesystem();
    //fs_test_shell();

    main_loop();
}
