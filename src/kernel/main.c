#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/stdio.h>
#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <drivers/keyboard.h>
#include <drivers/vga.h>

#include <boot/loader.h>
#include <kernel/test.h>

#include <kernel/pmm.h>

void kernel_main(uint32_t magic, uint32_t addr) 
{
	loader_init(magic, addr);

	gdt_init();
	idt_init();
	
	irq_register_handler(1, keyboard_callback);

	pmm_init(loader_get_memory_map(), loader_get_memory_map_length());
	
	// todo: move this to terminal file
    stdio_interface_t vga_interface = {
        .init = vga_initialize,
        .clear = vga_clear,
        .putc = vga_putchar,
        .puts = vga_writestring
    };
    stdio_set_interface(&vga_interface);
	stdio_init();
	printf("Welcome to Iddo and Hillel amazing os!!!!\n");
	test();

	// Print memory map
	multiboot_mmap_entry_t* mmap = loader_get_memory_map();
	uint32_t mmap_end = loader_get_memory_map_length() + (uintptr_t)mmap;
	while ((uintptr_t)mmap < (mmap_end)) {
    	printf("Region: base=0x%x%x, len=0x%x%x, type=%d\n",
           (uint32_t)(mmap->addr >> 32), (uint32_t)mmap->addr,
           (uint32_t)(mmap->len >> 32), (uint32_t)mmap->len,
           mmap->type);

   		mmap = (multiboot_mmap_entry_t*)((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
	}

	// Main Loop

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
