/*
 * include/mm/mmap.h - [Enter description]
 * Author:   amity
 * Date:     Tue Jul  7 12:41:26 2026
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
#ifndef __MM_MEMORY_MAP_H__
#define __MM_MEMORY_MAP_H__

/*
 * Canonical virtual address map. Every region a subsystem claims
 * must be listed here, with its neighbors, so overlap is caught by
 * reading this file rather than by a page fault six weeks later.
 *
 *   0x00000000 - 0x00100000   Low memory (BDA, EBDA, BIOS, VGA text)
 *   0x00100000 - identity_map_size    Kernel image + identity-mapped RAM
 *                                     (grows with installed RAM — see
 *                                     paging_get_identity_size())
 *   [heap_base, heap_base+HEAP_SIZE)  Kernel heap (mm/heap.c) — placed
 *                                     dynamically after kernel_end by
 *                                     heap_init(), NOT a fixed constant
 *   0xE0000000 - 0xE1C00000   VMM on-demand mapping window (mm/vmm.c)
 *   0xE2000000 - 0xE4000000   Kernel heap virtual arena (mm/heap.c) —
 *                             fixed range, physically non-contiguous
 *   0xFFC00000 - 0xFFFFF000   Recursive page table mapping
 *   0xFFFFF000 - 0x100000000  Recursive page directory (1 page)
 *
 * PCI MMIO BARs are NOT part of this map — they're wherever the
 * firmware/QEMU placed them (commonly 0xF0000000+) and are only ever
 * accessed through vmm_map_physical(), never assumed to sit at a
 * fixed address.
 */

#define VMM_WINDOW_BASE 0xE0000000u
#define VMM_WINDOW_SIZE 0x01C00000u

#define HEAP_VIRT_BASE 0xE2000000u
#define HEAP_VIRT_SIZE (32u * 1024u * 1024u)

#define RECURSIVE_PT_BASE 0xFFC00000u
#define RECURSIVE_PD_BASE 0xFFFFF000u

#define RECURSIVE_PT_BASE 0xFFC00000u
#define RECURSIVE_PD_BASE 0xFFFFF000u
/* --- Includes ---*/

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
#endif