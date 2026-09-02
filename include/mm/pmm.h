/*
 * mm/pmm.h - [Enter description]
 * Author:   amity
 * Date:     Mon Jun 15 09:29:00 2026
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

#ifndef __MM_PMM_H__
#define __MM_PMM_H__

#include <internal/multiboot.h>
#include <stddef.h>
#include <stdint.h>

#define FRAME_SIZE 4096
#define FRAME_ALIGN 4096
#define FRAME_SIZE_SHIFT 12
#define FRAME_SIZE_MASK (FRAME_SIZE - 1)
#define PMM_MIN_MEMORY (4 * 1024 * 1024)

typedef struct pmm_stats {
  PFN_T total_frames;
  PFN_T used_frames;
  PFN_T free_frames;
  PFN_T reserved_frames;
} pmm_stats_t;

typedef struct framebuffer_info {
  uint64_t addr;
  ULONG pitch;
  ULONG width;
  ULONG height;
  UCHAR bpp;
  UCHAR type;
  UCHAR valid;
  /* Only meaningful when type == 1 (RGB) */
  UCHAR red_pos, red_size;
  UCHAR green_pos, green_size;
  UCHAR blue_pos, blue_size;
} framebuffer_info_t;

typedef struct boot_info {
  ULONG magic;
  multiboot_info_t *mb_info;
  ULONGLONG total_ram;
  PFN_T total_frames;
  PHYS_ADDR_T kernel_start;
  PHYS_ADDR_T kernel_end;
  INT valid;
  framebuffer_info_t fb;
  VIRT_ADDR_T rsdp_addr; /* 0 if bootloader gave us none */
} boot_info_t;

VIRT_ADDR_T pmm_get_rsdp(VOID);
INT pmm_init(VOID);
VOID pmm_set_kernel_end(PHYS_ADDR_T end);

/*
 * Frame allocator. Returns PHYS_ADDR_INVALID (0) on failure.
 * Frame 0 is reserved with the rest of the first 1 MiB, so 0 is
 * never a successful allocation.
 */
PHYS_ADDR_T pmm_alloc_frame(VOID);
VOID pmm_free_frame(PHYS_ADDR_T frame);
PHYS_ADDR_T pmm_alloc_frames(ULONG count);
VOID pmm_free_frames(PHYS_ADDR_T frame, ULONG count);
PHYS_ADDR_T pmm_alloc_aligned(ULONG count, ULONG align_frames);
INT pmm_alloc_at(PHYS_ADDR_T addr, ULONG count);

LONG is_physical_address_mmio(PHYS_ADDR_T phys_addr);
VOID pmm_reserve_region(PHYS_ADDR_T start, SIZE_T length);
VOID pmm_unreserve_region(PHYS_ADDR_T start, SIZE_T length);
int pmm_is_region_free(PHYS_ADDR_T start, SIZE_T length);
VOID pmm_get_stats(pmm_stats_t *stats);
VOID pmm_print_map(void);
PFN_T pmm_get_total_frames(void);
PFN_T pmm_get_free_frames(void);
ULONGLONG pmm_get_total_ram(void);
const boot_info_t *pmm_get_boot_info(void);

#endif