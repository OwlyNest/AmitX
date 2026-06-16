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
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <internal/amitx_consts.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static boot_info_t boot_info;
static uint8_t *bitmap;
static uint32_t total_frames = 0;
static uint32_t used_frames = 0;
static uint32_t bitmap_size = 0;
static uint64_t total_ram = 0;

/* --- Prototypes ---*/
static inline void bitmap_set(uint32_t frame);
static inline void bitmap_clear(uint32_t frame);
static inline int bitmap_test(uint32_t frame);
static uint32_t find_first_free(void);
static uint32_t find_first_free_n(uint32_t count);

/* --- Functions ---*/

/* ==========================================================================
 * Bitmap bit manipulation
 * ======================================================================= */
static inline void bitmap_set(uint32_t frame) {
    bitmap[frame >> 3] |= (1 << (frame & 7));
}

static inline void bitmap_clear(uint32_t frame) {
    bitmap[frame >> 3] &= ~(1 << (frame & 7));
}

static inline int bitmap_test(uint32_t frame) {
    return bitmap[frame >> 3] & (1 << (frame & 7));
}

/* ==========================================================================
 * Find first free frame (naive linear scan)
 * ======================================================================= */
static uint32_t find_first_free(void) {
    for (uint32_t i = 0; i < bitmap_size; i++) {
        if (bitmap[i] == 0xFF)
            continue;
        for (uint32_t j = 0; j < 8; j++) {
            uint32_t frame = (i << 3) + j;
            if (frame >= total_frames)
                return (uint32_t)-1;
            if (!(bitmap[i] & (1 << j)))
                return frame;
        }
    }
    return (uint32_t)-1;
}

/* ==========================================================================
 * Find N contiguous free frames
 * ======================================================================= */
static uint32_t find_first_free_n(uint32_t count) {
    if (count == 0)
        return (uint32_t)-1;
    if (count == 1)
        return find_first_free();

    uint32_t run_start = 0;
    uint32_t run_len = 0;

    for (uint32_t i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            if (run_len == 0)
                run_start = i;
            run_len++;
            if (run_len >= count)
                return run_start;
        } else {
            run_len = 0;
        }
    }
    return (uint32_t)-1;
}

/* ==========================================================================
 * Reserve a region of physical memory (mark frames as used)
 * ======================================================================= */
void pmm_reserve_region(uintptr_t start, size_t length) {
    if (length == 0)
        return;

    uint32_t start_frame = start >> FRAME_SIZE_SHIFT;
    uint32_t end_frame = (start + length + FRAME_SIZE_MASK) >> FRAME_SIZE_SHIFT;

    if (end_frame > total_frames)
        end_frame = total_frames;

    for (uint32_t i = start_frame; i < end_frame; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_frames++;
        }
    }
}

/* ==========================================================================
 * Unreserve a region (mark frames as free)
 * ======================================================================= */
void pmm_unreserve_region(uintptr_t start, size_t length) {
    if (length == 0)
        return;

    uint32_t start_frame = start >> FRAME_SIZE_SHIFT;
    uint32_t end_frame = (start + length + FRAME_SIZE_MASK) >> FRAME_SIZE_SHIFT;

    if (end_frame > total_frames)
        end_frame = total_frames;

    for (uint32_t i = start_frame; i < end_frame; i++) {
        if (bitmap_test(i)) {
            bitmap_clear(i);
            used_frames--;
        }
    }
}

/* ==========================================================================
 * Early kernel initialization (called from boot.S before kernel_main)
 * Validates multiboot, detects RAM, saves boot info.
 * Returns 0 on success, non-zero on fatal error.
 * ======================================================================= */
int kernel_early_init(uint32_t magic, multiboot_info_t *mb_info) {
    memset(&boot_info, 0, sizeof(boot_info));
    boot_info.magic = magic;
    boot_info.mb_info = mb_info;
    boot_info.kernel_start = 0x00100000; /* Linker starts at 1MB */
    boot_info.kernel_end = (uint32_t)_end;

    /* Validate multiboot magic */
    if (!multiboot_valid_magic(magic)) {
        /* Can't printk yet, but we can return error */
        boot_info.valid = 0;
        return 1;
    }

    /* Check for memory map */
    if (!multiboot_has_mmap(mb_info)) {
        /* Fallback: use mem_lower/mem_upper (in KB, above 1MB) */
        if (multiboot_has_mem_info(mb_info)) {
            total_ram = ((uint64_t)mb_info->mem_upper + 1024) * 1024;
        } else {
            /* Absolute fallback: assume 16MB */
            total_ram = 16 * 1024 * 1024;
        }
    } else {
        /* Calculate total RAM from memory map */
        multiboot_mmap_entry_t *entry;
        uint64_t available_ram = 0;
        MULTIBOOT_MMAP_FOR_EACH(mb_info, entry) {
            if (entry->type == MMAP_AVAILABLE) {
                available_ram += entry->len;
            }
        }
        total_ram = available_ram;
    }

    /* Sanity check: do we have enough RAM? */
    if (total_ram < PMM_MIN_MEMORY) {
        boot_info.valid = 0;
        return 2;
    }

    boot_info.total_ram = total_ram;
    boot_info.valid = 1;
    return 0;
}

/* ==========================================================================
 * Initialize PMM from boot info (called after early init, during setup)
 * ======================================================================= */
static int pmm_init(void) {
    if (!boot_info.valid) {
        printk("[pmm] Boot info invalid, using 16MB fallback\n");
        total_ram = 16 * 1024 * 1024;
    }

    total_frames = (uint32_t)(total_ram >> FRAME_SIZE_SHIFT);
    if (total_frames == 0) {
        printk("[pmm] Warning: zero frames, using 16MB fallback\n");
        total_ram = 16 * 1024 * 1024;
        total_frames = total_ram >> FRAME_SIZE_SHIFT;
    }

    bitmap_size = (total_frames + 7) >> 3;

    /* Place bitmap right after kernel end, page-aligned */
    bitmap = (uint8_t *)(((uint32_t)_end + FRAME_ALIGN - 1) & ~(FRAME_ALIGN - 1));

    /* Mark everything as used, then free available regions */
    memset(bitmap, 0xFF, bitmap_size);
    used_frames = total_frames;

    /* Parse memory map and free available regions */
    if (boot_info.valid && multiboot_has_mmap(boot_info.mb_info)) {
        multiboot_mmap_entry_t *entry;
        MULTIBOOT_MMAP_FOR_EACH(boot_info.mb_info, entry) {
            if (entry->type == MMAP_AVAILABLE) {
                uint64_t addr = entry->addr;
                uint64_t len = entry->len;
                pmm_unreserve_region((uintptr_t)addr, (size_t)len);
            }
        }
    } else if (boot_info.valid && multiboot_has_mem_info(boot_info.mb_info)) {
        /* Fallback: free everything above 1MB up to mem_upper */
        uint32_t mem_upper_kb = boot_info.mb_info->mem_upper;
        pmm_unreserve_region(0x00100000, (size_t)mem_upper_kb * 1024);
    } else {
        /* Ultimate fallback: free 1MB-16MB */
        pmm_unreserve_region(0x00100000, 15 * 1024 * 1024);
    }

    /* Reserve 0-1MB (BIOS, VGA text buffer, EBDA, etc.) */
    pmm_reserve_region(0x00000000, 0x00100000);

    /* Reserve kernel area: 0x100000 to end of bitmap */
    uint32_t kernel_end = (uint32_t)bitmap + bitmap_size;
    pmm_reserve_region(0x00100000, kernel_end - 0x00100000);

    /* Reserve VGA text mode memory */
    pmm_reserve_region(VGA_MEM_PHYS, FRAME_SIZE);

    boot_info.total_frames = total_frames;
    boot_info.kernel_end = kernel_end;

    printk("[pmm] Initialized: %u total frames (%u MB), ...", total_frames, total_ram >> 20);
    printk("[pmm] Bitmap at 0x%08x, size %u bytes\n",
           (uint32_t)bitmap, bitmap_size);
    printk("[pmm] Kernel end: 0x%08x, Total RAM: %u MB\n",
           kernel_end, (uint32_t)(total_ram >> 20));
    return 0;
}

kscope_node_t pmm_node = {
    .name = "pmm",
    .id = 0x0007,
    .class = KSCOPE_CLASS_MEMORY,
    .subclass = KSCOPE_SUBCLASS_MEMORY_PMM,
    .requires = NULL,
    .require_count = 0,
    .provides = (const char *[]){"mem.physical", "mem.pages"},
    .provide_count = 2,
    .init = pmm_init
};

/* ==========================================================================
 * Allocate a single physical frame
 * ======================================================================= */
void *pmm_alloc_frame(void) {
    uint32_t frame = find_first_free();
    if (frame == (uint32_t)-1) {
        printk("[pmm] Out of memory!\n");
        return NULL;
    }

    bitmap_set(frame);
    used_frames++;
    return (void *)(frame << FRAME_SIZE_SHIFT);
}

/* ==========================================================================
 * Allocate N contiguous physical frames
 * ======================================================================= */
void *pmm_alloc_frames(uint32_t count) {
    uint32_t frame = find_first_free_n(count);
    if (frame == (uint32_t)-1) {
        printk("[pmm] Out of memory (contiguous %u frames)!\n", count);
        return NULL;
    }

    for (uint32_t i = 0; i < count; i++) {
        bitmap_set(frame + i);
        used_frames++;
    }
    return (void *)(frame << FRAME_SIZE_SHIFT);
}

/* ==========================================================================
 * Free a physical frame
 * ======================================================================= */
void pmm_free_frame(void *frame) {
    if (!frame)
        return;

    uint32_t f = (uint32_t)frame >> FRAME_SIZE_SHIFT;
    if (f >= total_frames)
        return;

    if (bitmap_test(f)) {
        bitmap_clear(f);
        used_frames--;
    }
}

/* ==========================================================================
 * Free N contiguous physical frames
 * ======================================================================= */
void pmm_free_frames(void *frame, uint32_t count) {
    if (!frame || count == 0)
        return;

    uint32_t f = (uint32_t)frame >> FRAME_SIZE_SHIFT;
    if (f + count > total_frames)
        return;

    for (uint32_t i = 0; i < count; i++) {
        if (bitmap_test(f + i)) {
            bitmap_clear(f + i);
            used_frames--;
        }
    }
}

/* ==========================================================================
 * Get PMM statistics
 * ======================================================================= */
void pmm_get_stats(pmm_stats_t *stats) {
    if (!stats)
        return;
    stats->total_frames = total_frames;
    stats->used_frames = used_frames;
    stats->free_frames = total_frames - used_frames;
    stats->reserved_frames = used_frames;
}

/* ==========================================================================
 * Print memory map summary
 * ======================================================================= */
void pmm_print_map(void) {
    printk("[pmm] Memory map:\n");
    printk("      Total frames:  %u (%u MB)\n",
           total_frames, (total_frames * FRAME_SIZE) >> 20);
    printk("      Used frames:   %u (%u MB)\n",
           used_frames, (used_frames * FRAME_SIZE) >> 20);
    printk("      Free frames:   %u (%u MB)\n",
           total_frames - used_frames,
           ((total_frames - used_frames) * FRAME_SIZE) >> 20);
}

/* ==========================================================================
 * Quick accessors
 * ======================================================================= */
uint32_t pmm_get_total_frames(void) {
    return total_frames;
}

uint32_t pmm_get_free_frames(void) {
    return total_frames - used_frames;
}

uint64_t pmm_get_total_ram(void) {
    return total_ram;
}

const boot_info_t *pmm_get_boot_info(void) {
    return &boot_info;
}