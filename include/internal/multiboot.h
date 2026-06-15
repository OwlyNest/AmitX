/*
	* include/multiboot.h - [Enter description]
	* Author:   amity
	* Date:     Mon Jun 15 09:13:19 2026
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

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

/* --- Includes ---*/
#include <stdint.h>

/* --- Macros ---*/
/* ==========================================================================
 * Multiboot header flags (what kernel requests from bootloader)
 * ======================================================================= */
#define MULTIBOOT_HEADER_MAGIC  0x1BADB002
#define MULTIBOOT_BOOT_MAGIC    0x2BADB002

#define MB_HEADER_PAGE_ALIGN    0x00000001
#define MB_HEADER_MEMORY_INFO   0x00000002
#define MB_HEADER_VIDEO_MODE    0x00000004
#define MB_HEADER_AOUT_KLUDGE   0x00010000

/* ==========================================================================
 * Multiboot info flags (what bootloader provides to kernel)
 * ======================================================================= */
#define MB_FLAG_MEM             0x00000001
#define MB_FLAG_BOOT_DEV        0x00000002
#define MB_FLAG_CMDLINE         0x00000004
#define MB_FLAG_MODS            0x00000008
#define MB_FLAG_AOUT            0x00000010
#define MB_FLAG_ELF             0x00000020
#define MB_FLAG_MMAP            0x00000040
#define MB_FLAG_DRIVES          0x00000080
#define MB_FLAG_CONFIG          0x00000100
#define MB_FLAG_LOADER_NAME     0x00000200
#define MB_FLAG_APM             0x00000400
#define MB_FLAG_VBE             0x00000800
#define MB_FLAG_FB              0x00001000

/* ==========================================================================
 * Memory map entry types
 * ======================================================================= */
#define MMAP_AVAILABLE          1
#define MMAP_RESERVED           2
#define MMAP_ACPI_RECLAIM       3
#define MMAP_NVS                4
#define MMAP_BADRAM             5

/* --- Typedefs - Structs - Enums ---*/

/* ==========================================================================
 * Multiboot information structure (passed by bootloader in EBX)
 * ======================================================================= */
typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint32_t vbe_mode;
    uint32_t vbe_interface_seg;
    uint32_t vbe_interface_off;
    uint32_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint32_t color_info;
} __attribute__((packed)) multiboot_info_t;

/* ==========================================================================
 * Memory map entry (array at mmap_addr)
 * Layout: size(4) | base_addr(8) | length(8) | type(4) = 24 bytes min
 * size field does NOT include itself; advance by size + 4
 * ======================================================================= */
typedef struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

/* --- Prototypes ---*/
static inline int multiboot_valid_magic(uint32_t magic);
static inline int multiboot_has_mmap(multiboot_info_t *mb);
static inline int multiboot_has_mem_info(multiboot_info_t *mb);
static inline int multiboot_has_framebuffer(multiboot_info_t *mb);

/* ==========================================================================
 * Inline validation helpers
 * ======================================================================= */
static inline int multiboot_valid_magic(uint32_t magic) {
    return magic == MULTIBOOT_BOOT_MAGIC;
}

static inline int multiboot_has_mmap(multiboot_info_t *mb) {
    return mb && (mb->flags & MB_FLAG_MMAP);
}

static inline int multiboot_has_mem_info(multiboot_info_t *mb) {
    return mb && (mb->flags & MB_FLAG_MEM);
}

static inline int multiboot_has_framebuffer(multiboot_info_t *mb) {
    return mb && (mb->flags & MB_FLAG_FB);
}

/* ==========================================================================
 * Iterate memory map entries
 * Usage:
 *   multiboot_mmap_entry_t *entry;
 *   MULTIBOOT_MMAP_FOR_EACH(mb_info, entry) {
 *       // use entry->addr, entry->len, entry->type
 *   }
 * ======================================================================= */
#define MULTIBOOT_MMAP_FOR_EACH(mb, entry)                          \
    for ((entry) = (multiboot_mmap_entry_t *)(uintptr_t)(mb)->mmap_addr; \
         (uint32_t)(entry) < (mb)->mmap_addr + (mb)->mmap_length;   \
         (entry) = (multiboot_mmap_entry_t *)((uint32_t)(entry) + (entry)->size + 4))

#endif