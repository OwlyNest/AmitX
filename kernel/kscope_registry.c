/*
	* kernel/kscope_registry.c - [Enter description]
	* Author:   amity
	* Date:     Tue Jun 16 13:54:20 2026
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
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>

#include <mm/paging.h>
#include <screen/printk.h>
#include <mm/pmm.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* -- Functions ---*/
void kscope_register_all(void) {
	kscope_register(&x86_gdt_node);
	kscope_register(&x86_pic_node);
	kscope_register(&x86_idt_node);

	kscope_register(&serial_node);
	kscope_register(&cpuid_node); // move back above SERIAL after CPUID is done
	kscope_register(&pit_timer_node);
	kscope_register(&keyboard_node);
	kscope_register(&screen_node);
	kscope_register(&pmm_node);
	kscope_register(&paging_node);
	kscope_register(&vmm_node);
	kscope_register(&heap_node);
	kscope_register(&scheduler_node);
	kscope_register(&pci_node);
	kscope_register(&acpi_node);
	kscope_register(&storage_node);
	kscope_register(&mouse_node);
	kscope_register(&e1000_node);
	kscope_register(&svga_node);
}