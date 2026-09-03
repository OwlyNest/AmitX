/*
 * mm/pmm.c - [Enter description]
 * Author:   amity
 * Date:     Mon Jun 15 09:28:58 2026
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
 * Architecture-agnostic: frame numbers are PFN_T (pointer-width) so
 * `(frame << 12)` cannot truncate above 4 GiB, and every physical
 * address is PHYS_ADDR_T. The bitmap still lives in identity-mapped
 * RAM placed just after the kernel image.
 */

/* --- Macros ---*/

/* --- Includes ---*/
#include "internal/multiboot.h"
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <internal/phonon_consts.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <screen/printk.h>
#include <stdint.h>
#include <sync/spinlock.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
boot_info_t boot_info;
static PUCHAR bitmap;
static PFN_T total_frames = 0;
static PFN_T used_frames = 0;
static SIZE_T bitmap_size = 0;
static ULONGLONG total_ram = 0;
static mb2_tag_mmap_t *g_mmap_tag = NULL;
static _SPINLOCK pmm_lock;

extern UCHAR _kernel_phys_end[];

/* --- Prototypes ---*/

/* --- Functions ---*/

/* ==========================================================================
 * Bitmap bit manipulation
 * ======================================================================= */
static inline void bitmap_set(PFN_T frame) {
  bitmap[frame >> 3] |= (UCHAR)(1u << (frame & 7));
}

static inline void bitmap_clear(PFN_T frame) {
  bitmap[frame >> 3] &= (UCHAR) ~(1u << (frame & 7));
}

static inline int bitmap_test(PFN_T frame) {
  return bitmap[frame >> 3] & (1u << (frame & 7));
}

/* ==========================================================================
 * Find first free frame (naive linear scan)
 * ======================================================================= */
static PFN_T find_first_free(void) {
  SIZE_T i;
  ULONG j;
  PFN_T frame;

  for (i = 0; i < bitmap_size; i++) {
    if (bitmap[i] == 0xFF)
      continue;
    for (j = 0; j < 8; j++) {
      frame = (PFN_T)((i << 3) + j);
      if (frame >= total_frames)
        return (PFN_T)-1;
      if (!(bitmap[i] & (1u << j)))
        return frame;
    }
  }
  return (PFN_T)-1;
}

/* ==========================================================================
 * Find N contiguous free frames
 * ======================================================================= */
static PFN_T find_first_free_n(ULONG count) {
  PFN_T run_start = 0;
  PFN_T run_len = 0;
  PFN_T i;

  if (count == 0)
    return (PFN_T)-1;
  if (count == 1)
    return find_first_free();

  for (i = 0; i < total_frames; i++) {
    if (!bitmap_test(i)) {
      if (run_len == 0)
        run_start = i;
      run_len++;
      if (run_len >= (PFN_T)count)
        return run_start;
    } else {
      run_len = 0;
    }
  }
  return (PFN_T)-1;
}

/* ==========================================================================
 * Reserve a region of physical memory (mark frames as used)
 * ======================================================================= */
VOID pmm_reserve_region(PHYS_ADDR_T start, SIZE_T length) {
  PFN_T start_frame;
  PFN_T end_frame;
  PFN_T i;
  ULONG flags;

  if (length == 0)
    return;

  start_frame = (PFN_T)(start >> FRAME_SIZE_SHIFT);
  end_frame = (PFN_T)((start + (PHYS_ADDR_T)length + FRAME_SIZE_MASK) >>
                      FRAME_SIZE_SHIFT);

  if (end_frame > total_frames)
    end_frame = total_frames;

  flags = spinlock_acquire(&pmm_lock);
  for (i = start_frame; i < end_frame; i++) {
    if (!bitmap_test(i)) {
      bitmap_set(i);
      used_frames++;
    }
  }
  spinlock_release(&pmm_lock, flags);
}

/* ==========================================================================
 * Unreserve a region (mark frames as free)
 * ======================================================================= */
VOID pmm_unreserve_region(PHYS_ADDR_T start, SIZE_T length) {
  PFN_T start_frame;
  PFN_T end_frame;
  PFN_T i;
  ULONG flags;

  if (length == 0)
    return;

  start_frame = (PFN_T)(start >> FRAME_SIZE_SHIFT);
  end_frame = (PFN_T)((start + (PHYS_ADDR_T)length + FRAME_SIZE_MASK) >>
                      FRAME_SIZE_SHIFT);

  if (end_frame > total_frames)
    end_frame = total_frames;

  flags = spinlock_acquire(&pmm_lock);
  for (i = start_frame; i < end_frame; i++) {
    if (bitmap_test(i)) {
      bitmap_clear(i);
      used_frames--;
    }
  }
  spinlock_release(&pmm_lock, flags);
}

INT pmm_is_region_free(PHYS_ADDR_T start, SIZE_T length) {
  PFN_T start_frame;
  PFN_T end_frame;
  PFN_T i;

  if (length == 0)
    return -1;

  start_frame = (PFN_T)(start >> FRAME_SIZE_SHIFT);
  end_frame = (PFN_T)((start + (PHYS_ADDR_T)length + FRAME_SIZE_MASK) >>
                      FRAME_SIZE_SHIFT);

  if (end_frame > total_frames)
    return -1;

  for (i = start_frame; i < end_frame; i++) {
    if (bitmap_test(i))
      return -1;
  }
  return 0;
}

/* ==========================================================================
 * Early kernel initialization (called from boot.S before kernel_main)
 * Validates multiboot, detects RAM, saves boot info.
 * Returns 0 on success, non-zero on fatal error.
 * ======================================================================= */
INT kernel_early_init(ULONG magic, PVOID mb_info) {
  mb2_tag_t *tag;
  ULONGLONG available_ram = 0;
  int has_mmap = 0;

  memset(&boot_info, 0, sizeof(boot_info));
  boot_info.magic = magic;
  boot_info.valid = 0;

  if (magic != MB2_BOOT_MAGIC)
    return 1;

  /* Max ACPI 2.0+ RSDP size. Lives in BSS so it's guaranteed mapped
   * before and after paging comes on, regardless of what happens to
   * the mb_info blob later. */
  static UCHAR g_rsdp_copy[36];

  MB2_TAG_FOREACH(mb_info, tag) {
    switch (tag->type) {
    case MB2_TAG_MMAP: {
      mb2_tag_mmap_t *mmap = (mb2_tag_mmap_t *)tag;

      if (mmap->entry_size < sizeof(mb2_mmap_entry_t))
        break;

      ULONG remaining = mmap->tag.size - 16;

      for (ULONG offset = 0; offset + mmap->entry_size <= remaining;
           offset += mmap->entry_size) {

        mb2_mmap_entry_t *e = (mb2_mmap_entry_t *)((PUCHAR)mmap + 16 + offset);

        if (e->type == MB2_MMAP_AVAILABLE)
          available_ram += e->length;
      }

      g_mmap_tag = mmap;
      has_mmap = 1;
      break;
    }
    case MB2_TAG_BASIC_MEM: {
      if (!has_mmap) {
        ULONG *mem = (ULONG *)(tag + 1);
        available_ram = ((ULONGLONG)mem[0] + mem[1]) * 1024;
      }
      break;
    }
    case MB2_TAG_FRAMEBUFFER: {
      mb2_tag_framebuffer_t *fb = (mb2_tag_framebuffer_t *)tag;

      boot_info.fb.addr = fb->addr;
      boot_info.fb.pitch = fb->pitch;
      boot_info.fb.width = fb->width;
      boot_info.fb.height = fb->height;
      boot_info.fb.bpp = fb->bpp;
      boot_info.fb.type = fb->type;
      boot_info.fb.valid = 1;

      if (fb->type == 1) {
        PUCHAR ci = fb->color_info;
        boot_info.fb.red_pos = ci[0];
        boot_info.fb.red_size = ci[1];
        boot_info.fb.green_pos = ci[2];
        boot_info.fb.green_size = ci[3];
        boot_info.fb.blue_pos = ci[4];
        boot_info.fb.blue_size = ci[5];
      }
      break;
    }
    case MB2_TAG_ACPI_NEW:
    case MB2_TAG_ACPI_OLD: {
      mb2_tag_acpi_t *acpi_tag = (mb2_tag_acpi_t *)tag;
      ULONG rsdp_len = tag->size - sizeof(mb2_tag_t);
      if (rsdp_len > sizeof(g_rsdp_copy))
        rsdp_len = sizeof(g_rsdp_copy);

      if (tag->type == MB2_TAG_ACPI_NEW || boot_info.rsdp_addr == 0) {
        memcpy(g_rsdp_copy, acpi_tag->rsdp, rsdp_len);
        boot_info.rsdp_addr = (VIRT_ADDR_T)g_rsdp_copy;
      }
      break;
    }
    }
  }

  if (available_ram == 0)
    available_ram = 16ULL * 1024ULL * 1024ULL;

  total_ram = available_ram;

  if (total_ram < PMM_MIN_MEMORY)
    return 2;

  boot_info.mb_info = (multiboot_info_t *)mb_info;
  boot_info.total_ram = total_ram;
  boot_info.valid = 1;
  return 0;
}

LONG is_physical_address_mmio(PHYS_ADDR_T phys_addr) {
  ULONG num_entries;
  ULONG i;
  mb2_mmap_entry_t *entries;

  if (!g_mmap_tag)
    return 1;

  num_entries = (g_mmap_tag->tag.size - 16) / g_mmap_tag->entry_size;
  entries = (mb2_mmap_entry_t *)(g_mmap_tag + 1);

  for (i = 0; i < num_entries; i++) {
    mb2_mmap_entry_t *e =
        (mb2_mmap_entry_t *)((PUCHAR)entries + i * g_mmap_tag->entry_size);

    if ((ULONGLONG)phys_addr >= e->base_addr &&
        (ULONGLONG)phys_addr < (e->base_addr + e->length)) {
      return 0;
    }
  }

  return 1;
}

/* ==========================================================================
 * Initialize PMM from boot info (called after early init, during setup)
 * ======================================================================= */
INT pmm_init(VOID) {
  PHYS_ADDR_T placement;
  PHYS_ADDR_T kernel_end;

  spinlock_init(&pmm_lock);

  if (!boot_info.valid)
    total_ram = 16ULL * 1024ULL * 1024ULL;

  total_frames = (PFN_T)(total_ram >> FRAME_SIZE_SHIFT);
  if (total_frames == 0) {
    total_ram = 16ULL * 1024ULL * 1024ULL;
    total_frames = (PFN_T)(total_ram >> FRAME_SIZE_SHIFT);
  }

  bitmap_size = (SIZE_T)((total_frames + 7) >> 3);

  /* Place bitmap right after kernel end, page-aligned.
   * _kernel_phys_end is a physical address; the identity map
   * (32-bit) / bootstrap identity window (64-bit) keeps this
   * pointer valid after paging_init(). */
  placement = ((PHYS_ADDR_T)(ULONG_PTR)_kernel_phys_end + FRAME_ALIGN - 1) &
              ~(PHYS_ADDR_T)(FRAME_ALIGN - 1);

  if (boot_info.mb_info) {
    PHYS_ADDR_T mb_end = ((PHYS_ADDR_T)(ULONG_PTR)boot_info.mb_info +
                          *(ULONG *)boot_info.mb_info + FRAME_ALIGN - 1) &
                         ~(PHYS_ADDR_T)(FRAME_ALIGN - 1);

    if (mb_end > placement)
      placement = mb_end;
  }

  bitmap = (PUCHAR)(VIRT_ADDR_T)placement;

  memset(bitmap, 0xFF, bitmap_size);
  used_frames = total_frames;

  if (boot_info.valid && boot_info.mb_info) {
    mb2_tag_t *tag;
    int found_mmap = 0;

    MB2_TAG_FOREACH(boot_info.mb_info, tag) {
      if (tag->type == MB2_TAG_MMAP) {
        mb2_tag_mmap_t *mmap = (mb2_tag_mmap_t *)tag;
        ULONG num = (mmap->tag.size - 16) / mmap->entry_size;
        mb2_mmap_entry_t *entry = (mb2_mmap_entry_t *)(mmap + 1);
        ULONG i;

        found_mmap = 1;
        for (i = 0; i < num; i++) {
          mb2_mmap_entry_t *e =
              (mb2_mmap_entry_t *)((PUCHAR)entry + i * mmap->entry_size);
          if (e->type == MB2_MMAP_AVAILABLE) {
            pmm_unreserve_region((PHYS_ADDR_T)e->base_addr, (SIZE_T)e->length);
          }
        }
        break;
      }
    }

    if (!found_mmap) {
      pmm_unreserve_region(0x00100000, (SIZE_T)(total_ram - 0x100000));
    }
  } else {
    pmm_unreserve_region(0x00100000, 15 * 1024 * 1024);
  }

  pmm_reserve_region(0x00000000, 0x00100000);

  kernel_end = (PHYS_ADDR_T)(VIRT_ADDR_T)bitmap + (PHYS_ADDR_T)bitmap_size;
  pmm_reserve_region(0x00100000, (SIZE_T)(kernel_end - 0x00100000));

  pmm_reserve_region((PHYS_ADDR_T)VGA_MEM_PHYS, FRAME_SIZE);

  boot_info.total_frames = total_frames;
  boot_info.kernel_end = kernel_end;

  return 0;
}

static const char *pmm_provides[] = {"mem.physical", "mem.pages"};

kscope_node_t pmm_node = {.name = "pmm",
                          .id = 0x0007,
                          .class = KSCOPE_CLASS_MEMORY,
                          .subclass = KSCOPE_SUBCLASS_MEMORY_PMM,
                          .requires = NULL,
                          .require_count = 0,
                          .provides = pmm_provides,
                          .provide_count = 2};

VOID pmm_set_kernel_end(PHYS_ADDR_T end) { boot_info.kernel_end = end; }
/* ==========================================================================
 * Allocate a single physical frame
 * ======================================================================= */
PHYS_ADDR_T pmm_alloc_frame(VOID) {
  PFN_T frame;
  ULONG flags;

  flags = spinlock_acquire(&pmm_lock);
  frame = find_first_free();
  if (frame == (PFN_T)-1) {
    spinlock_release(&pmm_lock, flags);
    return PHYS_ADDR_INVALID;
  }

  bitmap_set(frame);
  used_frames++;
  spinlock_release(&pmm_lock, flags);
  return (PHYS_ADDR_T)frame << FRAME_SIZE_SHIFT;
}

/* ==========================================================================
 * Allocate N contiguous physical frames
 * ======================================================================= */
PHYS_ADDR_T pmm_alloc_frames(ULONG count) {
  PFN_T frame;
  ULONG i;
  ULONG flags;

  flags = spinlock_acquire(&pmm_lock);
  frame = find_first_free_n(count);
  if (frame == (PFN_T)-1) {
    spinlock_release(&pmm_lock, flags);
    return PHYS_ADDR_INVALID;
  }

  for (i = 0; i < count; i++) {
    bitmap_set(frame + i);
    used_frames++;
  }
  spinlock_release(&pmm_lock, flags);
  return (PHYS_ADDR_T)frame << FRAME_SIZE_SHIFT;
}

/* ==========================================================================
 * Free a physical frame
 * ======================================================================= */
VOID pmm_free_frame(PHYS_ADDR_T frame) {
  PFN_T f;
  ULONG flags;

  if (!frame)
    return;

  f = (PFN_T)(frame >> FRAME_SIZE_SHIFT);
  if (f >= total_frames)
    return;

  flags = spinlock_acquire(&pmm_lock);
  if (bitmap_test(f)) {
    bitmap_clear(f);
    used_frames--;
  }
  spinlock_release(&pmm_lock, flags);
}

/* ==========================================================================
 * Free N contiguous physical frames
 * ======================================================================= */
VOID pmm_free_frames(PHYS_ADDR_T frame, ULONG count) {
  PFN_T f;
  ULONG i;
  ULONG flags;

  if (!frame || count == 0)
    return;

  f = (PFN_T)(frame >> FRAME_SIZE_SHIFT);
  if (f + (PFN_T)count > total_frames)
    return;

  flags = spinlock_acquire(&pmm_lock);
  for (i = 0; i < count; i++) {
    if (bitmap_test(f + i)) {
      bitmap_clear(f + i);
      used_frames--;
    }
  }
  spinlock_release(&pmm_lock, flags);
}

/* ==========================================================================
 * Get PMM statistics
 * ======================================================================= */
VOID pmm_get_stats(pmm_stats_t *stats) {
  if (!stats) {
    return;
  }
  stats->total_frames = total_frames;
  stats->used_frames = used_frames;
  stats->free_frames = total_frames - used_frames;
  stats->reserved_frames = used_frames;
}

VIRT_ADDR_T pmm_get_rsdp(VOID) { return boot_info.rsdp_addr; }

/* ==========================================================================
 * Print memory map summary
 * ======================================================================= */
void pmm_print_map(void) {
  printk("[pmm] Memory map:\n");
  printk("      Total frames:  %llu (%llu MB)\n",
         (unsigned long long)total_frames,
         (unsigned long long)((total_frames * FRAME_SIZE) >> 20));
  printk("      Used frames:   %llu (%llu MB)\n",
         (unsigned long long)used_frames,
         (unsigned long long)((used_frames * FRAME_SIZE) >> 20));
  printk(
      "      Free frames:   %llu (%llu MB)\n",
      (unsigned long long)(total_frames - used_frames),
      (unsigned long long)(((total_frames - used_frames) * FRAME_SIZE) >> 20));
}

/* ==========================================================================
 * Quick accessors
 * ======================================================================= */
PFN_T pmm_get_total_frames(void) { return total_frames; }

PFN_T pmm_get_free_frames(void) { return total_frames - used_frames; }

ULONGLONG pmm_get_total_ram(void) { return total_ram; }

const boot_info_t *pmm_get_boot_info(void) { return &boot_info; }

PHYS_ADDR_T pmm_alloc_aligned(ULONG count, ULONG align_frames) {
  PFN_T i;
  ULONG flags;

  if (count == 0 || align_frames == 0)
    return PHYS_ADDR_INVALID;

  if (align_frames & (align_frames - 1))
    return PHYS_ADDR_INVALID;

  if (align_frames == 1)
    return pmm_alloc_frames(count);

  flags = spinlock_acquire(&pmm_lock);
  i = 0;

  while (i < total_frames) {
    PFN_T aligned;
    ULONG j;
    int ok;

    while (i < total_frames && bitmap_test(i))
      i++;
    if (i >= total_frames)
      break;

    aligned = (i + (PFN_T)align_frames - 1) & ~((PFN_T)align_frames - 1);

    if (aligned + (PFN_T)count > total_frames)
      break;

    ok = 1;
    for (j = 0; j < count; j++) {
      if (bitmap_test(aligned + j)) {
        i = aligned + j + 1;
        ok = 0;
        break;
      }
    }

    if (ok) {
      for (j = 0; j < count; j++) {
        bitmap_set(aligned + j);
        used_frames++;
      }
      spinlock_release(&pmm_lock, flags);
      return (PHYS_ADDR_T)aligned << FRAME_SIZE_SHIFT;
    }
  }

  spinlock_release(&pmm_lock, flags);
  return PHYS_ADDR_INVALID;
}
int pmm_alloc_at(PHYS_ADDR_T addr, ULONG count) {
  PFN_T start_frame = (PFN_T)(addr >> FRAME_SIZE_SHIFT);
  ULONG i;
  ULONG flags;

  if (start_frame + (PFN_T)count > total_frames)
    return -1;

  flags = spinlock_acquire(&pmm_lock);
  for (i = 0; i < count; i++) {
    if (bitmap_test(start_frame + i)) {
      spinlock_release(&pmm_lock, flags);
      return -1;
    }
  }

  for (i = 0; i < count; i++) {
    bitmap_set(start_frame + i);
    used_frames++;
  }
  spinlock_release(&pmm_lock, flags);
  return 0;
}
