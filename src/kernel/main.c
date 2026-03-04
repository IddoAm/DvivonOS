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

void create_and_schedule_user_process() {
    printf("\n[PROCESS] Creating User Process...\n");
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    
    if (!proc) {
        printf("%o[PANIC] Failed to allocate process struct\n", STD_COLOR_LIGHT_RED);
        return;
    }
    memset(proc, 0, sizeof(*proc));

    // 1. Initialize Process Structure
    // Note: process_init now allocates a per-process kernel stack and updates TSS
    printf("[DEBUG] Calling process_init with Entry Point: 0x%x\n", PROCESS_HEAP_START);
    process_init(proc, (void*)PROCESS_HEAP_START);

    if (!proc->context) {
        printf("[PANIC] process_init failed to create context!\n");
        return;
    }

    // 2. Prepare User Code and Data
    const uint8_t user_code[] = {
        0xB8, 0x01, 0x00, 0x00, 0x00,    // mov eax, 1
        0xBB, 0x01, 0x00, 0x00, 0x00,    // mov ebx, 1
        0xB9, 0x00, 0x00, 0x00, 0x00,    // mov ecx, <PLACEHOLDER>
        0xBA, 0x10, 0x00, 0x00, 0x00,    // mov edx, 16 (Fixed length to include \n)
        0xCD, 0x67,                      // int 0x67
        0xEB, 0xE8                       // jmp short -24 (Fixed offset)
    };
    const char msg[] = "Hello from user\n"; 
    const size_t code_len = sizeof(user_code);
    const size_t msg_len = sizeof(msg) - 1;

    // Calculate Addresses
    const uint32_t code_vaddr = PROCESS_HEAP_START;
    const uint32_t msg_vaddr = code_vaddr + (uint32_t)code_len;

    // 3. Consolidate into a Single Payload
    // This prevents the VMM from overwriting the page when loading the second part.
    size_t total_payload_len = code_len + msg_len;
    uint8_t* payload = (uint8_t*)kmalloc(total_payload_len);
    if (!payload) {
        printf("[PANIC] Failed to allocate payload buffer\n");
        return;
    }

    // Copy code and message into the contiguous kernel buffer
    memcpy(payload, user_code, code_len);
    memcpy(payload + code_len, msg, msg_len);

    // Patch the mov ecx instruction (at offset 11) with the message's virtual address
    *(uint32_t*)&payload[11] = msg_vaddr;

    printf("[DEBUG] Memory Layout:\n");
    printf("        Code VAddr: 0x%x\n", code_vaddr);
    printf("        Msg  VAddr: 0x%x\n", msg_vaddr);
    printf("        Patched Addr in Payload: 0x%x\n", *(uint32_t*)&payload[11]);

    // 4. Load into Address Space in One Shot
    printf("[PROCESS] Loading consolidated payload into PD 0x%x...\n", proc->pd_phys);
    
    // We load the entire blob starting at the code's base address
    if (!process_load_user_memory(proc, code_vaddr, payload, total_payload_len)) {
        printf("[PANIC] Failed to load user memory!\n");
        kfree((uintptr_t)payload);
        return;
    }
    
    // Free the temporary kernel buffer
    kfree((uintptr_t)payload);
    printf("[DEBUG] User memory loaded successfully.\n");

    // 5. Handover to Scheduler
    printf("[SCHEDULER] Adding process to queue...\n");
    scheduler_add_process(proc);
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
