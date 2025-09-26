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

		printf("a");
		for(volatile int i=0;i<500000;i++);
	}
}

void shell_loop1(){
	printf("tests");
	while(true){

		printf("b");
		for(volatile int i=0;i<500000;i++);
	}
}

void kernel_main(uint32_t magic, uint32_t addr) 
{
	loader_init(magic, addr);

	gdt_init();
	idt_init();
	
	irq_register_handler(1, keyboard_callback);

	clock_init(100); // 100 Hz

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

	pmm_init(loader_get_memory_map(), loader_get_memory_map_length());
	vmm_init();

	scheduler_init();
	task_t shell_task1;
	uint32_t* shell_stack1 = (uint32_t*)kmalloc(2*PAGE_SIZE);
	task_init(&shell_task1, shell_loop2, shell_stack1 + (2*PAGE_SIZE)/sizeof(uint32_t));

	task_t shell_task2;
	uint32_t* shell_stack2 = (uint32_t*)kmalloc(2*PAGE_SIZE);
	task_init(&shell_task2, shell_loop1, shell_stack2 + (2*PAGE_SIZE)/sizeof(uint32_t));

	start_first_task();
	
	printf("test");
}


