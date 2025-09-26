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

void kernel_main(uint32_t magic, uint32_t addr) 
{
	loader_init(magic, addr);

	gdt_init();
	idt_init();
	
	irq_register_handler(1, keyboard_callback);

	clock_init(5); // 100 Hz

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

	scheduler_init();

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
