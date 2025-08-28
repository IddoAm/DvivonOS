#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <drivers/keyboard.h>

#include <boot/loader.h>
#include <kernel/pmm.h>

#include <prog/terminal.h>

void kernel_main(uint32_t magic, uint32_t addr) 
{
	loader_init(magic, addr);

	gdt_init();
	idt_init();
	
	irq_register_handler(1, keyboard_callback);

	terminal_initialize();

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
	

	pmm_init(loader_get_memory_map(), loader_get_memory_map_length());
	

	// Main Loop

	key_event event;

	while(true){
		if(keyboard_read(&event)){
			if(event.type == KEY_CHAR){
				terminal_keypress(event);
			}
		}
		__asm__ volatile ("hlt");
	}
}
