/*
 * include/mm/paging.h - Paging API (i386 2-level / x86_64 4-level)
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
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

/* --- Macros ---*/
#ifndef __MM_PAGING_H__
#define __MM_PAGING_H__

#include "internal/phonon_types.h"
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_MASK 0xFFFu

#if ARCH_X86_64
#define PT_ENTRIES 512
#define PD_ENTRIES 512
#define PDPT_ENTRIES 512
#define PML4_ENTRIES 512
typedef ULONGLONG PTE_T;
#else
#define PT_ENTRIES 1024
#define PD_ENTRIES 1024
typedef ULONG PTE_T;
#endif

/* Page table entry flags (low 12 bits, plus NX on long mode) */
#define PAGE_PRESENT 0x001
#define PAGE_WRITABLE 0x002
#define PAGE_USER 0x004
#define PAGE_WRITETHRU 0x008
#define PAGE_NOCACHE 0x010
#define PAGE_ACCESSED 0x020
#define PAGE_DIRTY 0x040
#define PAGE_HUGE 0x080 /* PS: 2 MiB page in a PD entry */
#define PAGE_GLOBAL 0x100

#if ARCH_X86_64
#define PAGE_NX (1ULL << 63)
#else
#define PAGE_NX 0 /* no NX without PAE */
#endif

#define PAGE_NO_EXECUTE PAGE_NX

#define PAGE_FLAG_MASK ((ULONG64)0xFFFu | (ULONG64)PAGE_NX)

/*
 * Physical address from a PTE. On long mode bits 12-51 are the
 * frame; bit 63 is NX and must not leak into the address.
 */
#if ARCH_X86_64
#define PTE_ADDR(entry) ((PHYS_ADDR_T)(entry) & 0x000FFFFFFFFFF000ULL)
#else
#define PTE_ADDR(entry) ((PHYS_ADDR_T)(entry) & ~(PHYS_ADDR_T)0xFFFu)
#endif

#define PTE_FLAGS(entry) ((ULONG)(entry) & 0xFFFu)

#if ARCH_X86_64

#define PML4_VIRT ((PTE_T *)(RECURSIVE_PML4_BASE))

#define PDPT_VIRT(pml4)                                                        \
  ((PTE_T *)(RECURSIVE_PDPT_BASE +                                             \
             ((VIRT_ADDR_T)(pml4) * (VIRT_ADDR_T)PAGE_SIZE)))

#define PD_VIRT_AT(pml4, pdpt)                                                 \
  ((PTE_T *)(RECURSIVE_PD_BASE + ((VIRT_ADDR_T)(pml4) << 21) +                 \
             ((VIRT_ADDR_T)(pdpt) * (VIRT_ADDR_T)PAGE_SIZE)))

#define PT_VIRT_AT(pml4, pdpt, pd)                                             \
  ((PTE_T *)(RECURSIVE_PT_BASE + ((VIRT_ADDR_T)(pml4) << 30) +                 \
             ((VIRT_ADDR_T)(pdpt) << 21) +                                     \
             ((VIRT_ADDR_T)(pd) * (VIRT_ADDR_T)PAGE_SIZE)))

#else /* ARCH_X86_32 */

#define PT_VIRT(pd_idx)                                                        \
  ((PTE_T *)(RECURSIVE_PT_BASE + ((VIRT_ADDR_T)(pd_idx) * PAGE_SIZE)))
#define PD_VIRT ((PTE_T *)RECURSIVE_PD_BASE)

#endif
/* --- Includes ---*/
#include <mm/mmap.h>
#include <stddef.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
INT paging_init(VOID);
INT map_page(PHYS_ADDR_T phys, VIRT_ADDR_T virt, ULONG64 flags);
VOID unmap_page(VIRT_ADDR_T virt);
PHYS_ADDR_T virt_to_phys(VIRT_ADDR_T virt);
SIZE_T paging_get_identity_size(VOID);
#endif