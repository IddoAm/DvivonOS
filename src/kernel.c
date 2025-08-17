#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"

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
	terminal_initialize();
	terminal_writestring("Welcome to Iddo and Hillel amazing os!!!!\n");

	gdt_init();
	idt_init();
	
	irq_register_handler(1, keyboard_callback);

	key_event event;

	while(true){

		if(keyboard_read(&event)){
			if(event.type == KEY_CHAR){
				terminal_putchar(event.c);
			}
		}
		__asm__ volatile ("hlt");
	}
	
}
