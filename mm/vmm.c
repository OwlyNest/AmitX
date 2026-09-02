/*
 * mm/vmm.c - On-demand physical-to-virtual mapping window
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
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

/*
 * Architecture-agnostic. The window base/size come from mmap.h, so
 * this file does not hard-code 0xE0000000 (which is user-half on
 * long mode). Flags are ULONG64 so PAGE_NX (bit 63) survives.
 */

/* --- Macros ---*/
/*
 * IMPORTANT: this window must NOT overlap real hardware MMIO. On
 * QEMU/VirtualBox, PCI BARs commonly live in 0xF0000000-0xFFFFFFFF
 * (see your own SVGA/e1000 BAR addresses: 0xfd000000, 0xfe000000,
 * 0xfeb80000). 0xE0000000 sits clear of that range, clear of
 * KERNEL_VIRT_BASE (0xC0000000), and clear of the recursive mapping
 * (0xFFC00000+).
 */
#define VMM_WINDOW_PAGES (VMM_WINDOW_SIZE / PAGE_SIZE)
/* --- Includes ---*/
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <lib/string.h>
#include <mm/mmap.h>
#include <mm/paging.h>
#include <mm/vmm.h>
#include <screen/printk.h>
#include <stddef.h>
#include <stdint.h>
#include <sync/spinlock.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static UCHAR window_bitmap[(VMM_WINDOW_PAGES + 7) / 8];
static _SPINLOCK vmm_lock;
/* --- Prototypes ---*/

/* --- Functions ---*/

/* ==========================================================================
 *                                                                          *
 * Bitmap helpers, same shape as pmm.c's, but tracking virtual pages        *
 * within our own window, not physical frames                               *
 *                                                                          *
 * =======================================================================  */
static inline VOID bit_set(ULONG i) {
  window_bitmap[i >> 3] |= (UCHAR)(1u << (i & 7));
}

static inline VOID bit_clear(ULONG i) {
  window_bitmap[i >> 3] &= (UCHAR) ~(1u << (i & 7));
}

static inline INT bit_test(ULONG i) {
  return window_bitmap[i >> 3] & (1u << (i & 7));
}

static ULONG find_free_run(ULONG count) {
  ULONG run_start = 0;
  ULONG run_len = 0;
  ULONG i;

  for (i = 0; i < (ULONG)VMM_WINDOW_PAGES; i++) {
    if (!bit_test(i)) {
      if (run_len == 0)
        run_start = i;
      run_len++;
      if (run_len >= count)
        return run_start;
    } else {
      run_len = 0;
    }
  }
  return (ULONG)-1;
}
/* ==========================================================================
 *                                                                          *
 * Initialization                                                           *
 *                                                                          *
 * =======================================================================  */
static INT vmm_init(VOID) {
  memset(window_bitmap, 0, sizeof(window_bitmap));
  spinlock_init(&vmm_lock);
  printk("[vmm] Mapping window: %p - %p (%u MB)\n",
         (PVOID)(VIRT_ADDR_T)VMM_WINDOW_BASE,
         (PVOID)(VIRT_ADDR_T)(VMM_WINDOW_BASE + VMM_WINDOW_SIZE),
         (unsigned)(VMM_WINDOW_SIZE >> 20));
  return 0;
}

kscope_node_t vmm_node = {
    .name = "vmm",
    .id = 0x0012,
    .class = KSCOPE_CLASS_MEMORY,
    .subclass = KSCOPE_SUBCLASS_MEMORY_VMM,
    .requires = (kscope_node_t *[]){&pmm_node, &paging_node},
    .require_count = 2,
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
PVOID vmm_map_physical(PHYS_ADDR_T phys, SIZE_T length, ULONG64 flags) {
  PHYS_ADDR_T phys_start;
  PHYS_ADDR_T phys_end;
  ULONG pages;
  ULONG flags_saved;
  ULONG start;
  VIRT_ADDR_T virt_start;
  ULONG i;

  if (length == 0)
    return NULL;

  phys_start = phys & ~(PHYS_ADDR_T)(PAGE_SIZE - 1);
  phys_end = (phys + (PHYS_ADDR_T)length + PAGE_SIZE - 1) &
             ~(PHYS_ADDR_T)(PAGE_SIZE - 1);
  pages = (ULONG)((phys_end - phys_start) / PAGE_SIZE);

  flags_saved = spinlock_acquire(&vmm_lock);

  start = find_free_run(pages);
  if (start == (ULONG)-1) {
    spinlock_release(&vmm_lock, flags_saved);
    printk("[vmm] No free window space for %u pages\n", pages);
    return NULL;
  }

  /* Reserve the run immediately so a concurrent caller can't pick
     the same pages before map_page() below finishes */
  for (i = 0; i < pages; i++)
    bit_set(start + i);

  spinlock_release(&vmm_lock, flags_saved);

  virt_start = (VIRT_ADDR_T)VMM_WINDOW_BASE + (VIRT_ADDR_T)start * PAGE_SIZE;

  for (i = 0; i < pages; i++) {
    if (map_page(phys_start + (PHYS_ADDR_T)i * PAGE_SIZE,
                 virt_start + (VIRT_ADDR_T)i * PAGE_SIZE, flags) != 0) {
      ULONG undo_flags = spinlock_acquire(&vmm_lock);
      ULONG j;
      for (j = 0; j < i; j++) {
        unmap_page(virt_start + (VIRT_ADDR_T)j * PAGE_SIZE);
        bit_clear(start + j);
      }
      bit_clear(start + i);
      spinlock_release(&vmm_lock, undo_flags);
      return NULL;
    }
  }

  return (PVOID)(virt_start + (VIRT_ADDR_T)(phys - phys_start));
}

/* ==========================================================================
 *                                                                          *
 * Unmap a range previously returned by vmm_map_physical                    *
 *                                                                          *
 * =======================================================================  */
VOID vmm_unmap_physical(PVOID virt, SIZE_T length) {
  VIRT_ADDR_T v;
  VIRT_ADDR_T v_end;
  ULONG start;
  ULONG pages;
  ULONG i;
  ULONG flags_saved;

  if (!virt || length == 0)
    return;

  v = (VIRT_ADDR_T)virt & ~(VIRT_ADDR_T)(PAGE_SIZE - 1);
  v_end = ((VIRT_ADDR_T)virt + length + PAGE_SIZE - 1) &
          ~(VIRT_ADDR_T)(PAGE_SIZE - 1);

  if (v < (VIRT_ADDR_T)VMM_WINDOW_BASE ||
      v_end > (VIRT_ADDR_T)VMM_WINDOW_BASE + (VIRT_ADDR_T)VMM_WINDOW_SIZE) {
    printk("[vmm] unmap: %p not in mapping window\n", virt);
    return;
  }

  start = (ULONG)((v - (VIRT_ADDR_T)VMM_WINDOW_BASE) / PAGE_SIZE);
  pages = (ULONG)((v_end - v) / PAGE_SIZE);

  for (i = 0; i < pages; i++)
    unmap_page(v + (VIRT_ADDR_T)i * PAGE_SIZE);

  flags_saved = spinlock_acquire(&vmm_lock);
  for (i = 0; i < pages; i++)
    bit_clear(start + i);
  spinlock_release(&vmm_lock, flags_saved);
}
