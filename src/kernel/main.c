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

void kernel_main(uint32_t magic, uint32_t addr) 
{
	for(volatile int i=0;i<10000000;i++);

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
	/*
	loader_init(magic, addr);

	gdt_init();
	idt_init();
	
	isr_register_handler(irq_to_vector(1), keyboard_callback);

	clock_init(100); // 100 Hz
	*/	
	while(true) {
		
	}
}


