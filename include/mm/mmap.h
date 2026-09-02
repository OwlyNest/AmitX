/*
 * include/mm/mmap.h - Canonical virtual address map
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
 * Every region a subsystem claims must be listed here, with its
 * neighbors, so overlap is caught by reading this file rather than
 * by a page fault six weeks later.
 *
 * 32-bit (i386, 2-level paging, recursive PD[1023]):
 *
 *   0x00000000 - 0x00100000   Low memory (BDA, EBDA, BIOS, VGA text)
 *   0x00100000 - identity     Kernel image + identity-mapped RAM
 *   0xC0000000 - 0xE0000000   Higher-half kernel image
 *   0xE0000000 - 0xE1C00000   VMM on-demand mapping window
 *   0xE2000000 - 0xE4000000   Kernel heap virtual arena
 *   0xFFC00000 - 0xFFFFF000   Recursive page tables
 *   0xFFFFF000 - 0x100000000  Recursive page directory (1 page)
 *
 * 64-bit (long mode, 4-level, recursive PML4[511]):
 *
 *   0x0000000000000000 - 0x00007FFFFFFFFFFF   Lower half (user / bootstrap
 *                                             identity window)
 *   0xFFFF800000000000 - +phys RAM            Direct physical map
 *   0xFFFF900000000000 - +32 MiB              VMM window
 *   0xFFFF910000000000 - +32 MiB              Kernel heap arena
 *   0xFFFFFF0000000000 - 0xFFFFFF7FBFDFE000   Recursive paging
 *                                             (PML4[510], not 511 —
 *                                             511 is the kernel image)
 *   0xFFFFFFFF80000000 - 0xFFFFFFFFFFFFFFFF    Kernel image (-2 GiB)

 *
 * PCI MMIO BARs are NOT part of this map — they are only ever
 * accessed through vmm_map_physical().
 */
#if ARCH_X86_64

/* Higher-half kernel at -2 GiB (Linux-style). */
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL

/* Direct physical map. Covers the first 64 TiB of phys RAM, 1:1. */
#define DIRECT_MAP_BASE 0xFFFF800000000000ULL
#define DIRECT_MAP_SIZE (64ULL << 40)

/*
 * VMM on-demand window — kernel space, clear of the direct map
 * and of the kernel image.
 */
#define VMM_WINDOW_BASE 0xFFFF900000000000ULL
#define VMM_WINDOW_SIZE 0x0000000002000000ULL /* 32 MiB */

/* Kernel heap virtual arena. Physically non-contiguous. */
#define HEAP_VIRT_BASE 0xFFFF910000000000ULL
#define HEAP_VIRT_SIZE (32u * 1024u * 1024u)

/*
 * Recursive mapping via PML4[510], leaving PML4[511] for the
 * -2 GiB kernel image (0xFFFFFFFF80000000). Using 511 for both
 * is a silent overlap: the recursive slot *is* the kernel's
 * top-level entry.
 *
 *   page tables      @ 0xFFFFFF0000000000
 *   page directories @ 0xFFFFFF7F80000000
 *   PDPTs            @ 0xFFFFFF7FBFC00000
 *   PML4 itself      @ 0xFFFFFF7FBFDFE000
 */
#define RECURSIVE_PML4_INDEX 510u
#define RECURSIVE_PT_BASE                                                      \
  (0xFFFF000000000000ULL | ((ULONGLONG)RECURSIVE_PML4_INDEX << 39))
#define RECURSIVE_PD_BASE                                                      \
  (RECURSIVE_PT_BASE | ((ULONGLONG)RECURSIVE_PML4_INDEX << 30))
#define RECURSIVE_PDPT_BASE                                                    \
  (RECURSIVE_PD_BASE | ((ULONGLONG)RECURSIVE_PML4_INDEX << 21))
#define RECURSIVE_PML4_BASE                                                    \
  (RECURSIVE_PDPT_BASE | ((ULONGLONG)RECURSIVE_PML4_INDEX << 12))

#else /* ARCH_X86_32 */

#define KERNEL_VIRT_BASE 0xC0000000u

#define VMM_WINDOW_BASE 0xE0000000u
#define VMM_WINDOW_SIZE 0x01C00000u

#define HEAP_VIRT_BASE 0xE2000000u
#define HEAP_VIRT_SIZE (32u * 1024u * 1024u)

#define RECURSIVE_PT_BASE 0xFFC00000u
#define RECURSIVE_PD_BASE 0xFFFFF000u

#endif

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

#endif
