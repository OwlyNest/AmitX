/*
 * include/internal/kscope_nodes.h - [Enter description]
 * Author:   amity
 * Date:     Tue Jun 16 13:51:17 2026
 * Copyright © 2026 OwlyNest
 */

/* --- Styling Instructions ---
 * Encoding:                      UTF-8, Unix line endings
 * Text font:                     Monospace
 * Line width:                    Max 80 characters
 * Indentation:                   Use 4 spaces
 * Brace style:                   Same line as control statement
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

/* --- Macros ---*/
#ifndef __INTERNAL_KSCOPE_NODES_H__
#define __INTERNAL_KSCOPE_NODES_H__
/* --- Includes ---*/
#include "internal/phonon_types.h"
#include <internal/kscope.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern kscope_node_t x86_gdt_node;
extern kscope_node_t x86_pic_node;
extern kscope_node_t x86_idt_node;
extern kscope_node_t pit_timer_node;
extern kscope_node_t keyboard_node;
extern kscope_node_t pmm_node;
extern kscope_node_t heap_node;
extern kscope_node_t screen_node;
extern kscope_node_t pci_node;
extern kscope_node_t acpi_node;
extern kscope_node_t e1000_node;
extern kscope_node_t serial_node;
extern kscope_node_t mouse_node;
extern kscope_node_t storage_node;
extern kscope_node_t svga_node;

extern kscope_node_t scheduler_node;
extern kscope_node_t vmm_node;
extern kscope_node_t paging_node;
extern kscope_node_t cpuid_node;

/* --- Prototypes ---*/
#endif