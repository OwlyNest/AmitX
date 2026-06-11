/*
	* kernel/init.c - [Enter description]
	* Author:   amity
	* Date:     Wed Jun 10 10:32:15 2026
	* Copyright © 2026 OwlyNest
*/

/* --- Styling Instructions ---
	* Encoding:                      UTF-8, Unix line endings
	* Text font:                     Monospace
	* Line width:                    Max 80 characters
	* Indentation:                   Use 4 spaces
	* Brace style:                   Same line as control statement
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include "screen.h"
#include "io.h"
#include "amfs.h"
#include "serial.h"
#include "timer.h"
#include "keyboard.h"
#include "acpi.h"
#include "interrupts.h"
#include "pci.h"
#include "ide.h"
#include "heap.h"
#include "idt.h"
#include "syscall.h"
#include "printk.h"
#include "fs.h"
#include "mouse.h"
#include "kernel.h"
#include "task.h"
#include "amitx_consts.h"
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern void gdt_install();
/* --- Prototypes ---*/

/* --- Functions ---*/
void kernel_setup(void) {
    gdt_install();
    pic_remap();
    idt_install();
	__asm__ __volatile__("sti");
    register_interrupt_handler(0, divide_by_zero_handler);
    init_keyboard();
    init_timer(100);
    fs_init();
    init_tasks();
    syscall_init();
    init_mouse();
	serial_init();
	heap_init();
	pci_init();
	pci_print_all_devices();
	acpi_init();
    acpi_print_info();

	pci_device_t* ide = pci_get_device(0x8086, 0x7010);
	if (ide) {
        ide_init(0x1F0, 0x3F6);
        uint16_t identify[256];
        if (ide_identify(0, identify) == 0) {
            // Try mount first
            if (amfs_mount() != 0) {
                // No filesystem — format it
                amfs_mkfs(10 * 2048);  // 10MB = 20480 sectors
                amfs_mount();
                
                // Create initial files
                amfs_write_file("helloworld.txt", "Hello from AmitFS!\n", 19);
                amfs_write_file("README", "AmitX Filesystem v0.1\n", 22);
            }
            
            // List and read back
            amfs_ls();
            
            char buf[256];
            int len = amfs_read_file("helloworld.txt", buf, sizeof(buf));
            if (len > 0) {
                buf[len] = '\0';
                printk("[amitfs] Read back: %s", buf);
            }
        }
    }

    setcolor(15, 0);
    clear();
}


