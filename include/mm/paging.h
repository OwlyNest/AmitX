/*
	* include/mm/paging.c - [Enter description]
	* Author:   amity
	* Date:     Mon Jun 22 10:57:43 2026
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
#ifndef __MM_PAGING_H__
#define __MM_PAGING_H__

#define PAGE_SIZE 		 4096
#define PAGE_SHIFT 		 12
#define PAGE_MASK 		 0xFFF

#define PD_ENTRIES 		 1024
#define PT_ENTRIES 		 1024

/* Page table entry flags */
#define PAGE_PRESENT 	 0x001
#define PAGE_WRITABLE 	 0x002
#define PAGE_USER 		 0x004
#define PAGE_WRITETHRU 	 0x008
#define PAGE_NOCACHE 	 0x010
#define PAGE_ACCESSED 	 0x020
#define PAGE_DIRTY 		 0x040
#define PAGE_GLOBAL      0x100

/* Extract physical address or flags from a PDE/PTE */
#define PTE_ADDR(entry)  ((entry) & ~0xFFF)
#define PTE_FLAGS(entry) ((entry) & 0xFFF)

/* --- Includes ---*/
#include <stdint.h>
#include <stddef.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
void paging_init(void);
int map_page(uintptr_t phys, uintptr_t virt, uint32_t flags);
void unmap_page(uintptr_t virt);
uintptr_t virt_to_phys(uintptr_t virt);
size_t paging_get_identity_size(void);
#endif