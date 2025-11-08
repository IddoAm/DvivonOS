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
	loader_init(magic, addr);

	pmm_init(loader_get_memory_map(), loader_get_memory_map_length());
	vmm_init();
	gdt_init();
	idt_init();
	//vmm_map_kernel_hh(0xB8000, 0xB8000, PAGE_PRESENT | PAGE_RW);
	pmm_remap_bitmap((uintptr_t)_pmm_bitmap_start + KERNEL_HIGHER_HALF);

	vmm_remove_identity_mapping();
	after_vmm:
	//isr_register_handler(irq_to_vector(1), keyboard_callback);

	clock_init(100); // 100 Hz

	stdio_interface_t vga_interface = {
        .init = vga_initialize,
        .clear = vga_clear,
        .putc = vga_putchar,
        .puts = vga_writestring
    };
    stdio_set_interface(&vga_interface);
	stdio_init();
	printf("Welcome to Iddo and Hillel amazing os!!!!\n");
	int* x = (int*)kmalloc(sizeof(int));
	*x = 123456;
	printf("kmalloc test: %d\n", *x);
	for(volatile int i=0;i<1000000000;i++);
	printf("phys address of x: %x\n", vmm_virt_to_phys((uint32_t)x));

	/*

	task_t shell_task1;
	uint32_t* shell_stack1 = (uint32_t*)kmalloc(4*PAGE_SIZE);
	uint32_t shell_pd1 = vmm_create_page_directory();
	task_init(&shell_task1, shell_loop1, shell_stack1 + (4*PAGE_SIZE)/sizeof(uint32_t), shell_pd1);

	printf("la\n");

	task_t shell_task2;
	uint32_t* shell_stack2 = (uint32_t*)kmalloc(4*PAGE_SIZE);
	uint32_t shell_pd2 = vmm_create_page_directory();
	task_init(&shell_task2, shell_loop2, shell_stack2 + (4*PAGE_SIZE)/sizeof(uint32_t), shell_pd2);

	task_t shell_task3;
	uint32_t* shell_stack3 = (uint32_t*)kmalloc(4*PAGE_SIZE);
	uint32_t shell_pd3 = vmm_create_page_directory();
	task_init(&shell_task3, shell_loop3, shell_stack3 + (4*PAGE_SIZE)/sizeof(uint32_t), shell_pd3);
	
	//scheduler_init();

	*/

	printf("la\n");
	printf("bambam");
	while(true) {
		
	}
}


