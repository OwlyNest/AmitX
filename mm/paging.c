/*
 * mm/paging.c - x86 32-bit paging with recursive mapping
 * Author:   amity
 * Date:     Mon Jun 22 10:45:00 2026
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

/* --- Includes ---*/
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <internal/virtmem.h>
#include <lib/string.h>
#include <mm/paging.h>
#include <mm/pmm.h>
#include <screen/printk.h>
#include <stdint.h>
#include <sync/spinlock.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
/* Physical address of page directory, set during init.
 * After paging is on, use 0xFFFFF000 (recursive mapping) instead. */
static uintptr_t page_directory_phys = 0;
static size_t identity_map_size = 0;
static spinlock_t paging_lock;
extern uint8_t __kernel_phys_start[];
extern uint8_t _kernel_phys_end[];
/* --- Prototypes ---*/

/* --- Functions ---*/
/* ==========================================================================
 * Initialize paging
 *
 * Creates a page directory and two page tables to identity-map the
 * physical memory. This covers VGA (0xB8000), the
 * kernel (0x100000), the PMM bitmap, and the heap.
 * ======================================================================= */
int paging_init(void) {
  spinlock_init(&paging_lock);

  uint32_t *pd = (uint32_t *)pmm_alloc_frame();
  if (!pd) {
    return -1;
  }
  memset(pd, 0, PAGE_SIZE);
  page_directory_phys = (uintptr_t)pd;

  /* Identity-map as much RAM as we have, but never touch slot 1023 */
  size_t pts_needed = ((size_t)(pmm_get_total_ram() >> 22)) + 1;
  if (pts_needed > 1023)
    pts_needed = 1023;
  identity_map_size = pts_needed * 4u * 1024u * 1024u;

  for (uint32_t pd_idx = 0; pd_idx < pts_needed; pd_idx++) {
    uint32_t *pt = (uint32_t *)pmm_alloc_frame();
    if (!pt)
      return -1;
    memset(pt, 0, PAGE_SIZE);

    for (int j = 0; j < PT_ENTRIES; j++) {
      uint32_t phys = (pd_idx * PT_ENTRIES + j) * PAGE_SIZE;
      pt[j] = phys | PAGE_PRESENT | PAGE_WRITABLE;
    }
    pd[pd_idx] = ((uint32_t)pt) | PAGE_PRESENT | PAGE_WRITABLE;
  }

  /* Higher-half mapping of the kernel image – done on the physical PD/PTs
   * before recursive mapping and before CR3 is loaded. */
  uintptr_t phys_start = (uintptr_t)__kernel_phys_start;
  uintptr_t phys_end = ((uintptr_t)_kernel_phys_end + 0xFFF) & ~0xFFF;

  for (uintptr_t p = phys_start; p < phys_end; p += PAGE_SIZE) {
    uintptr_t v = p + 0xC0000000;
    uint32_t pd_idx = v >> 22;
    uint32_t pt_idx = (v >> 12) & 0x3FF;

    /* Allocate a page table for this PD slot if we don’t have one yet */
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
      uint32_t *pt = (uint32_t *)pmm_alloc_frame();
      if (!pt)
        return -1;
      memset(pt, 0, PAGE_SIZE);
      pd[pd_idx] = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITABLE;
    }

    uint32_t *pt = (uint32_t *)(pd[pd_idx] & ~0xFFF);
    pt[pt_idx] = p | PAGE_PRESENT | PAGE_WRITABLE;
  }

  /* NOW install recursive mapping and switch */
  pd[1023] = page_directory_phys | PAGE_PRESENT | PAGE_WRITABLE;
  __asm__ __volatile__("mov %0, %%cr3" : : "r"(page_directory_phys) : "memory");
  /* … enable PG in CR0 … */

  uint32_t cr0;
  __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
  cr0 |= 0x80000000;
  __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0) : "memory");

  /* ------------------------------------------------------------------ */
  /* Immediate verification that does not need printk or the IDT.       */
  /* If this hangs with EAX = 0xDEADRECU the recursive entry is gone.   */
  /* ------------------------------------------------------------------ */
  volatile uint32_t *rec = (volatile uint32_t *)0xFFFFF000;
  if ((rec[1023] & 1) == 0) {
    __asm__ __volatile__("mov $0xDEAD5EC0, %%eax; hlt" ::: "eax");
  }

  /* Tell PMM the highest frame we used */
  uintptr_t kernel_phys_end =
      ((uintptr_t)_kernel_phys_end + PAGE_MASK) & ~(uintptr_t)PAGE_MASK;
  uintptr_t paging_end = page_directory_phys + (pts_needed + 1 + 4) * PAGE_SIZE;
  if (paging_end < kernel_phys_end)
    paging_end = kernel_phys_end;
  pmm_set_kernel_end((paging_end + FRAME_ALIGN - 1) & ~(FRAME_ALIGN - 1));

  return 0;
}

static kscope_node_t *paging_requires[] = {&pmm_node};

static const char *paging_provides[] = {"mem.paging"};

kscope_node_t paging_node = {.name = "paging",
                             .id = 0x13,
                             .class = KSCOPE_CLASS_MEMORY,
                             .subclass = KSCOPE_SUBCLASS_MEMORY_PAGING,
                             .requires = paging_requires,
                             .require_count = 1,
                             .provides = paging_provides,
                             .provide_count = 1,
                             .init = paging_init};

/* ==========================================================================
 * Map one physical page to a virtual page
 *
 * 'phys' and 'virt' must be page-aligned (bottom 12 bits zero).
 * 'flags' is PAGE_WRITABLE and/or PAGE_USER. PAGE_PRESENT is added
 * automatically. Returns 0 on success, -1 on error.
 * ======================================================================= */
int map_page(uintptr_t phys, uintptr_t virt, uint32_t flags) {
  if (phys & PAGE_MASK) {
    printk("[paging] map_page: phys 0x%08x not aligned\n", (uint32_t)phys);
    return -1;
  }
  if (virt & PAGE_MASK) {
    printk("[paging] map_page: virt 0x%08x not aligned\n", (uint32_t)virt);
    return -1;
  }

  /* bits 31-22: page directory index
   * bits 21-12: page table index
   * bits 11-0 : offset within page */
  uint32_t pd_idx = (uint32_t)(virt >> 22);
  uint32_t pt_idx = (uint32_t)((virt >> 12) & 0x3FFu);

  uint32_t saved_flags = spinlock_acquire(&paging_lock);

  /* If no page table exists for this PD slot, allocate one.
   * Locked so two callers racing on the same never-before-used
   * 4MB region can't both allocate a page table and clobber each
   * other's PD entry. */
  if (!(PD_VIRT[pd_idx] & PAGE_PRESENT)) {
    uintptr_t new_pt_phys = (uintptr_t)pmm_alloc_frame();
    if (!new_pt_phys) {
      spinlock_release(&paging_lock, saved_flags);
      return -1;
    }

    /* Point PD entry at the new page table (physical) */
    PD_VIRT[pd_idx] = (uint32_t)new_pt_phys | PAGE_PRESENT | PAGE_WRITABLE;

    /* Flush TLB for the page table's virtual address so we
     * can access it through the recursive mapping */
    uintptr_t pt_virt_addr = 0xFFC00000u + ((uintptr_t)pd_idx * PAGE_SIZE);
    __asm__ __volatile__("invlpg (%0)" : : "r"(pt_virt_addr) : "memory");

    /* Now clear the new page table via its virtual address */
    memset(PT_VIRT(pd_idx), 0, PAGE_SIZE);
  }

  /* Write the PTE */
  PT_VIRT(pd_idx)
  [pt_idx] = ((uint32_t)(phys & ~(uintptr_t)PAGE_MASK)) | (flags & 0xFFFu) |
             PAGE_PRESENT;

  /* Flush TLB for the mapped page */
  __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");

  spinlock_release(&paging_lock, saved_flags);

  return 0;
}

/* ==========================================================================
 * Unmap a virtual page
 * ======================================================================= */
void unmap_page(uintptr_t virt) {
  uint32_t pd_idx = (uint32_t)(virt >> 22);
  uint32_t pt_idx = (uint32_t)((virt >> 12) & 0x3FFu);

  uint32_t saved_flags = spinlock_acquire(&paging_lock);

  if (!(PD_VIRT[pd_idx] & PAGE_PRESENT)) {
    spinlock_release(&paging_lock, saved_flags);
    return;
  }

  PT_VIRT(pd_idx)[pt_idx] = 0;

  __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");

  spinlock_release(&paging_lock, saved_flags);
}

/* ==========================================================================
 * Translate virtual address to physical
 * Returns 0 if not mapped.
 * ======================================================================= */
uintptr_t virt_to_phys(uintptr_t virt) {
  uint32_t pd_idx = (uint32_t)(virt >> 22);
  uint32_t pt_idx = (uint32_t)((virt >> 12) & 0x3FFu);

  if (!(PD_VIRT[pd_idx] & PAGE_PRESENT)) {
    return 0;
  }

  if (!(PT_VIRT(pd_idx)[pt_idx] & PAGE_PRESENT)) {
    return 0;
  }

  return (uintptr_t)(PTE_ADDR(PT_VIRT(pd_idx)[pt_idx]) |
                     (uint32_t)(virt & PAGE_MASK));
}

size_t paging_get_identity_size(void) { return identity_map_size; }