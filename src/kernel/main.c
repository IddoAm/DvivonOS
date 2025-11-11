#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/stdio.h>
#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <drivers/keyboard.h>
#include <drivers/vga.h>

#include <boot/loader.h>

#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/heap_allocator.h>
#include <kernel/time/time.h>
#include <kernel/scheduler/scheduler.h>

#include<arch/i686/pic.h>

void shell_loop2(){
	printf("tests");
	while(true){

		printf("#");
		for(volatile int i=0;i<1000000;i++);
	}
}

void shell_loop1(){
	printf("tests");
	while(true){

		printf("-");
		for(volatile int i=0;i<1000000;i++);
	}
}

void shell_loop3(){
	printf("tests");
	while(true){

		printf("+");
		for(volatile int i=0;i<1000000;i++);
	}
}

void kernel_main(uint32_t magic, uint32_t virt_addr, uint32_t phys_addr) 
{
	// todo: move this to terminal file
    stdio_interface_t vga_interface = {
        .init = vga_initialize,
        .clear = vga_clear,
        .putc = vga_putchar,
        .puts = vga_writestring
    };
    stdio_set_interface(&vga_interface);
	stdio_init();
	printf("Welcome to Iddo's and hillel's amazing OS\n");

	loader_init(magic, virt_addr, phys_addr);

	
	multiboot_mmap_entry_t* mmap = loader_get_memory_map();
	uint32_t mmap_end = loader_get_memory_map_length() + (uintptr_t)mmap;
	while ((uintptr_t)mmap < (mmap_end)) {
    	printf("Region: base=0x%x%x, len=0x%x%x, type=%d\n",
           (uint32_t)(mmap->addr >> 32), (uint32_t)mmap->addr,
           (uint32_t)(mmap->len >> 32), (uint32_t)mmap->len,
           mmap->type);

   		mmap = (multiboot_mmap_entry_t*)((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
	}
	

	gdt_init();
	idt_init();

	pmm_init(loader_get_memory_map(), loader_get_memory_map_length());

	uint32_t* allocation = (uint32_t*)kmalloc(sizeof(uint32_t));
	*allocation = 5;
	printf("%d, %x\n", *allocation, allocation);
	uint32_t* allocation2 = (uint32_t*)kmalloc(sizeof(uint32_t));
	*allocation2 = 10;
	printf("%d, %x\n", *allocation2, allocation2);
	printf("%d, %x\n", *allocation, allocation);
	printf("freeing second allocation\n");

	kfree((uintptr_t)allocation2);


	allocation2 = (uint32_t*)kmalloc(sizeof(uint32_t));
	*allocation2 = 15;
	printf("%d, %x\n", *allocation2, allocation2);
	printf("%d, %x\n", *allocation, allocation);
	kfree((uintptr_t)allocation);
	kfree((uintptr_t)allocation2);
	
	isr_register_handler(irq_to_vector(1), keyboard_callback);
	pic_clear_mask(1);

	clock_init(100); // 100 Hz
	key_event event;

	while(true){

		if(keyboard_read(&event)){
			if(event.type == KEY_CHAR){
				putc(event.c);
			}
		}
		__asm__ volatile ("hlt");
	}
}


