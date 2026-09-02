/*
 * include/virtmem.h - Physical/virtual translation helpers
 * Author:   amity
 * Date:     Mon Jun 15 12:58:16 2026
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

#ifndef __INTERNAL_VIRTMEM_H__
#define __INTERNAL_VIRTMEM_H__

/* --- Includes ---*/
#include "internal/phonon_types.h"
#include <internal/phonon_consts.h>
#include <mm/mmap.h>
#include <mm/paging.h>
#include <stddef.h>
#include <stdint.h>

/* --- Macros ---*/
#ifndef KERNEL_VIRT_BASE
#error "KERNEL_VIRT_BASE must come from mm/mmap.h"
#endif

/*
 * Physical-to-virtual and virtual-to-physical translation.
 *
 * 32-bit: kernel image lives at phys + 0xC0000000. Low RAM is
 *         identity-mapped, so auto_virt() prefers that window.
 *
 * 64-bit: PHYS_TO_VIRT is the direct map (DIRECT_MAP_BASE + phys).
 *         The kernel image itself is at KERNEL_VIRT_BASE + phys
 *         (Linux-style -2 GiB); use KERNEL_PHYS_TO_VIRT for that.
 */

#if ARCH_X86_64

#define PHYS_TO_VIRT(addr)                                                     \
  ((PVOID)(DIRECT_MAP_BASE + (VIRT_ADDR_T)(PHYS_ADDR_T)(addr)))
#define VIRT_TO_PHYS(addr)                                                     \
  ((PHYS_ADDR_T)((VIRT_ADDR_T)(addr) - DIRECT_MAP_BASE))
#define KERNEL_PHYS_TO_VIRT(addr)                                              \
  ((PVOID)(KERNEL_VIRT_BASE + (VIRT_ADDR_T)(PHYS_ADDR_T)(addr)))

#else

#define PHYS_TO_VIRT(addr)                                                     \
  ((PVOID)((VIRT_ADDR_T)(PHYS_ADDR_T)(addr) + (VIRT_ADDR_T)KERNEL_VIRT_BASE))
#define VIRT_TO_PHYS(addr)                                                     \
  ((PHYS_ADDR_T)((VIRT_ADDR_T)(addr) - (VIRT_ADDR_T)KERNEL_VIRT_BASE))
#define KERNEL_PHYS_TO_VIRT(addr) PHYS_TO_VIRT(addr)

#endif

#define VGA_VIRT_ADDR (KERNEL_VIRT_BASE + VGA_MEM_PHYS)

/* --- Prototypes ---*/

/* ==========================================================================
 * Check if paging is currently enabled (reads CR0.PG bit)
 * ======================================================================= */
FORCEINLINE int paging_enabled(void) {
  ULONG_PTR cr0;
  __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
  return (cr0 & 0x80000000u) != 0;
}

/* ==========================================================================
 * Auto-detect: return a usable pointer for a physical address.
 *
 * Before paging: physical addresses are used directly.
 * After paging:  we identity-map the first 8 MB, so low physical
 * addresses still work as direct pointers. For addresses above 8 MB,
 * we assume they are mapped at KERNEL_VIRT_BASE offset.
 *
 * This is a temporary helper until we do a proper higher-half kernel.
 * ======================================================================= */
FORCEINLINE PVOID auto_virt(PHYS_ADDR_T phys_addr) {
  if (!paging_enabled()) {
    return (PVOID)(VIRT_ADDR_T)phys_addr;
  }
  if (phys_addr < (PHYS_ADDR_T)paging_get_identity_size()) {
    return (PVOID)(VIRT_ADDR_T)phys_addr;
  }
  return PHYS_TO_VIRT(phys_addr);
}

/* ==========================================================================
 * Get VGA memory address (auto-detects paging state)
 * ======================================================================= */
FORCEINLINE USHORT *vga_memory(VOID) {
  return (USHORT *)auto_virt(VGA_MEM_PHYS);
}

#endif