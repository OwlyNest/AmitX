/*
	* include/virtmem.h - [Enter description]
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
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

#ifndef VIRTMEM_H
#define VIRTMEM_H

/* --- Includes ---*/
#include <mm/paging.h>
#include <stdint.h>
#include <stddef.h>
#include <internal/amitx_consts.h>

/* --- Macros ---*/
/* ==========================================================================
 * Higher-half kernel base address
 * ======================================================================= */
#define KERNEL_VIRT_BASE        0xC0000000

/* ==========================================================================
 * Physical-to-virtual and virtual-to-physical translation
 * These are identity-mapped when paging is off, and offset by KERNEL_VIRT_BASE
 * when paging is on (in higher-half mode).
 * ======================================================================= */
#define PHYS_TO_VIRT(addr)      ((void *)(((uintptr_t)(addr)) + (uintptr_t)KERNEL_VIRT_BASE))
#define VIRT_TO_PHYS(addr)      ((uintptr_t)(addr) - (uintptr_t)KERNEL_VIRT_BASE)

/* ==========================================================================
 * Common virtual addresses (physical + KERNEL_VIRT_BASE)
 * ======================================================================= */
#define VGA_VIRT_ADDR           (KERNEL_VIRT_BASE + VGA_MEM_PHYS)

/* --- Prototypes ---*/

/* ==========================================================================
 * Check if paging is currently enabled (reads CR0.PG bit)
 * ======================================================================= */
static inline int paging_enabled(void) {
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    return (cr0 & 0x80000000) != 0;
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
static inline void *auto_virt(uintptr_t phys_addr) {
    if (!paging_enabled()) {
        return (void *)phys_addr;
    }
    /* Identity-mapped region: 0 to 8 MB */
    if (phys_addr < paging_get_identity_size()) {
        return (void *)phys_addr;
    }
    /* High physical addresses: assume mapped at KERNEL_VIRT_BASE.
     * (Only works if you actually map them there with map_page!) */
    return PHYS_TO_VIRT(phys_addr);
}

/* ==========================================================================
 * Get VGA memory address (auto-detects paging state)
 * ======================================================================= */
static inline uint16_t *vga_memory(void) {
    return (uint16_t *)auto_virt(VGA_MEM_PHYS);
}

#endif