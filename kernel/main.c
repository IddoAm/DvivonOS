#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/stdio.h>
#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <drivers/keyboard.h>
#include <drivers/vga.h>

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

void kernel_main(void) 
{
	
	gdt_init();
	idt_init();
	
	irq_register_handler(1, keyboard_callback);
	
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
