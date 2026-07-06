/*
	* mm/vmm.c - [Enter description]
	* Author:   amity
	* Date:     Mon Jul  6 15:39:35 2026
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
#include "internal/kscope.h"
#define VMM_WINDOW_BASE     0xFE000000u
#define VMM_WINDOW_SIZE     0x01C00000u        /* 28 MB, ends at 0xFFC00000 */
#define VMM_WINDOW_PAGES    (VMM_WINDOW_SIZE / PAGE_SIZE)
/* --- Includes ---*/
#include <mm/vmm.h>
#include <mm/paging.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <stdint.h>
#include <stddef.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static uint8_t window_bitmap[(VMM_WINDOW_PAGES + 7) / 8];
/* --- Prototypes ---*/

/* --- Functions ---*/

/* ==========================================================================
 *                                                                          *
 * Bitmap helpers, same shape as pmm.c's, but tracking virtual pages        *
 * within our own window, not physical frames                               *
 *                                                                          *
 * =======================================================================  */
static inline void bit_set(uint32_t i) {
	window_bitmap[i >> 3] |= (1 << (i & 7));
}

static inline void bit_clear(uint32_t i) {
	window_bitmap[i >> 3] &= ~(1 << (i & 7));
}

static inline int bit_test(uint32_t i) {
    return window_bitmap[i >> 3] & (1 << (i & 7));
}

static uint32_t find_free_run(uint32_t count) {
	uint32_t run_start = 0;
    uint32_t run_len = 0;

    for (uint32_t i = 0; i < VMM_WINDOW_PAGES; i++) {
        if (!bit_test(i)) {
            if (run_len == 0) run_start = i;
            run_len++;
            if (run_len >= count) return run_start;
        } else {
            run_len = 0;
        }
    }
    return (uint32_t)-1;
}

/* ==========================================================================
 *                                                                          *
 * Initialization                                                           *
 *                                                                          *
 * =======================================================================  */
static int vmm_init(void) {
	memset(window_bitmap, 0, sizeof(window_bitmap));
	printk("[vmm] Mapping window: 0x%08x - 0x%08x (%u MB)\n", VMM_WINDOW_BASE, VMM_WINDOW_BASE + VMM_WINDOW_SIZE, VMM_WINDOW_SIZE >> 20);
	return 0;
}

kscope_node_t vmm_node = {
    .name = "vmm",
    .id = 0x0012,
    .class = KSCOPE_CLASS_MEMORY,
    .subclass = KSCOPE_SUBCLASS_MEMORY_VMM,
    .requires = (kscope_node_t*[]){&pmm_node},
    .require_count = 1,
    .provides = (const char *[]){"mem.virtual", "mem.pages"},
    .provide_count = 2,
	.init = vmm_init,
};

/* ==========================================================================
 *                                                                          *
 * Map an arbitrary physical range and return a pointer usable for the      *
 * caller's original (possibly unaligned) address                           *
 *                                                                          *
 * =======================================================================  */
 void *vmm_map_physical(uintptr_t phys, size_t length, uint32_t flags) {
    if (length == 0) return NULL;

    uintptr_t phys_start = phys & ~(uintptr_t)(PAGE_SIZE - 1);
    uintptr_t phys_end = (phys + length + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    uint32_t pages = (uint32_t)((phys_end - phys_start) / PAGE_SIZE);

    uint32_t start = find_free_run(pages);
    if (start == (uint32_t)-1) {
        printk("[vmm] No free window space for %u pages\n", pages);
        return NULL;
    }

    uintptr_t virt_start = VMM_WINDOW_BASE + (uintptr_t)start * PAGE_SIZE;

    for (uint32_t i = 0; i < pages; i++) {
        if (map_page(phys_start + (uintptr_t)i * PAGE_SIZE, virt_start + (uintptr_t)i * PAGE_SIZE, flags) != 0) {
            /* Unwind whatever succeeded before the failure */
            for (uint32_t j = 0; j < i; j++) {
                unmap_page(virt_start + (uintptr_t)j * PAGE_SIZE);
                bit_clear(start + j);
            }
            return NULL;
        }
        bit_set(start + i);
    }

    return (void *)(virt_start + (phys - phys_start));
}

/* ==========================================================================
 *                                                                          *
 * Unmap a range previously returned by vmm_map_physical                    *
 *                                                                          *
 * =======================================================================  */
void vmm_unmap_physical(void *virt, size_t length) {
    if (!virt || length == 0) return;

    uintptr_t v = (uintptr_t)virt & ~(uintptr_t)(PAGE_SIZE - 1);
    uintptr_t v_end = ((uintptr_t)virt + length + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);

    if (v < VMM_WINDOW_BASE || v_end > VMM_WINDOW_BASE + VMM_WINDOW_SIZE) {
        printk("[vmm] unmap: 0x%08x not in mapping window\n", (uint32_t)virt);
        return;
    }

    uint32_t start = (uint32_t)((v - VMM_WINDOW_BASE) / PAGE_SIZE);
    uint32_t pages = (uint32_t)((v_end - v) / PAGE_SIZE);

    for (uint32_t i = 0; i < pages; i++) {
        unmap_page(v + (uintptr_t)i * PAGE_SIZE);
        bit_clear(start + i);
    }
}