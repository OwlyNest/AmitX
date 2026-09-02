/*
 * include/mm/vmm.h - On-demand physical-to-virtual mapping window
 * Author:   amity
 * Date:     Mon Jul  6 15:39:41 2026
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
#ifndef __MM_VMM_H__
#define __MM_VMM_H__
/* --- Includes ---*/
#include <stddef.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
PVOID vmm_map_physical(PHYS_ADDR_T phys, SIZE_T length, ULONG64 flags);
VOID vmm_unmap_physical(PVOID virt, SIZE_T length);
#endif