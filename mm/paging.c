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
    * Inline comments:               Column 40, wherever possible, else, whole multiple of 20
    * Section headers:               Use 3 '-' characters before and after
    * Pointer notation:              Next to variable name, not type
    * Binary operations:             Space around operator
    * Empty parameter list:          Use (void) instead of ()
    * Statements and declarations:   Max one per line
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include <mm/paging.h>
#include <mm/pmm.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
/* Physical address of page directory, set during init.
 * After paging is on, use 0xFFFFF000 (recursive mapping) instead. */
static uintptr_t page_directory_phys = 0;
static size_t identity_map_size = 0;
/* --- Prototypes ---*/

/* --- Functions ---*/
/* ==========================================================================
 * Initialize paging
 *
 * Creates a page directory and two page tables to identity-map the
 * physical memory. This covers VGA (0xB8000), the
 * kernel (0x100000), the PMM bitmap, and the heap.
 * ======================================================================= */
void paging_init(void) {
    /* Allocate page directory (must be page-aligned).
     * pmm_alloc_frame() returns a physical address. Paging is off,
     * so physical addresses are directly usable as pointers. */
    uint32_t *pd = (uint32_t *)pmm_alloc_frame();
    if (!pd) {
        printk("[paging] Failed to allocate page directory\n");
        return;
    }

    /* Clear all 1024 entries. Zero = not present. */
    memset(pd, 0, PAGE_SIZE);
    page_directory_phys = (uintptr_t)pd;

    /* Identity-map RAM */
    size_t pts_needed = ((size_t)(pmm_get_total_ram() >> 22)) + 1;
    identity_map_size = pts_needed * 4u * 1024u * 1024u;

    for (uint32_t pd_idx = 0; pd_idx < pts_needed; pd_idx++) {
        uint32_t *pt = (uint32_t *)pmm_alloc_frame();
        if (!pt) {
            printk("[paging] Failed to allocate page table %d\n", pd_idx);
            return;
        }

        /* Clear the page table */
        memset(pt, 0, PAGE_SIZE);

        /* Fill 1024 entries, mapping 4 MB of physical memory */
        for (int pt_idx = 0; pt_idx < PT_ENTRIES; pt_idx++) {
            uint32_t phys_addr = (pd_idx * PT_ENTRIES + pt_idx) * PAGE_SIZE;
            pt[pt_idx] = phys_addr | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        }

        /* Point PD entry at this PT. Address must be physical. */
        pd[pd_idx] = ((uint32_t)pt) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }

    /* Recursive mapping: last PD entry points to PD itself.
     * After this, we NEVER use 'pd' as a pointer again.
     * We use PD_VIRT (0xFFFFF000) instead. */
    pd[1023] = page_directory_phys | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    /* Load CR3 with physical address of PD */
    printk("PD phys=%08x\n", (uint32_t)pd);

    printk("PT0 phys=%08x\n", (uint32_t)(pd[0] & ~0xFFF));
    printk("PT1 phys=%08x\n", (uint32_t)(pd[1] & ~0xFFF));

    printk("CR3=%08x\n", page_directory_phys);
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(page_directory_phys));

    /* Enable paging */
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    printk("Before PG\n");
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0));
    printk("After PG\n");

    printk("[paging] Enabled. Identity-mapped 0x00000000-0x007FFFFF\n");
}

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

    /* If no page table exists for this PD slot, allocate one */
    if (!(PD_VIRT[pd_idx] & PAGE_PRESENT)) {
        uintptr_t new_pt_phys = (uintptr_t)pmm_alloc_frame();
        if (!new_pt_phys) {
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
    PT_VIRT(pd_idx)[pt_idx] = ((uint32_t)(phys & ~(uintptr_t)PAGE_MASK)) | (flags & 0xFFFu) | PAGE_PRESENT;

    /* Flush TLB for the mapped page */
    __asm__ __volatile__("invlpg (%0)"
                         : : "r"(virt) : "memory");

    return 0;
}

/* ==========================================================================
 * Unmap a virtual page
 * ======================================================================= */
void unmap_page(uintptr_t virt) {
    uint32_t pd_idx = (uint32_t)(virt >> 22);
    uint32_t pt_idx = (uint32_t)((virt >> 12) & 0x3FFu);

    if (!(PD_VIRT[pd_idx] & PAGE_PRESENT)) {
        return;
    }

    PT_VIRT(pd_idx)[pt_idx] = 0;

    __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
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

    return (uintptr_t)(PTE_ADDR(PT_VIRT(pd_idx)[pt_idx]) | (uint32_t)(virt & PAGE_MASK));
}

size_t paging_get_identity_size(void) {
    return identity_map_size;
}