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
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <internal/multiboot.h>

#define FRAME_SIZE              4096
#define FRAME_ALIGN             4096
#define FRAME_SIZE_SHIFT        12
#define FRAME_SIZE_MASK         (FRAME_SIZE - 1)
#define PMM_MIN_MEMORY          (4 * 1024 * 1024)

typedef struct pmm_stats {
    uint32_t total_frames;
    uint32_t used_frames;
    uint32_t free_frames;
    uint32_t reserved_frames;
} pmm_stats_t;

typedef struct boot_info {
    uint32_t        magic;
    multiboot_info_t *mb_info;
    uint64_t        total_ram;
    uint32_t        total_frames;
    uint32_t        kernel_start;
    uint32_t        kernel_end;
    int             valid;
} boot_info_t;

void *pmm_alloc_frame(void);
void pmm_free_frame(void *frame);
void *pmm_alloc_frames(uint32_t count);
void pmm_free_frames(void *frame, uint32_t count);
void pmm_reserve_region(uintptr_t start, size_t length);
void pmm_unreserve_region(uintptr_t start, size_t length);
void pmm_get_stats(pmm_stats_t *stats);
void pmm_print_map(void);
uint32_t pmm_get_total_frames(void);
uint32_t pmm_get_free_frames(void);
uint64_t pmm_get_total_ram(void);
const boot_info_t *pmm_get_boot_info(void);
void *pmm_alloc_aligned(uint32_t count, uint32_t align_frames);
int pmm_alloc_at(uintptr_t addr, uint32_t count);

#endif