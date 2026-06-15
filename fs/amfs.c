/*
	* fs/amfs.c - [Enter description]
	* Author:   amity
	* Date:     Thu Jun 11 18:12:44 2026
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
#include "fs/amfs.h"
#include "hw/ide.h"
#include "screen/printk.h"
#include "lib/string.h"
#include "mm/heap.h"
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
static amfs_superblock_t sb;
static int mounted = 0;
/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

static int read_sector(uint32_t lba, void *buf) {
	return ide_read_sectors(0, lba, 1, (uint16_t *)buf);
}

static int write_sector(uint32_t lba, const void *buf) {
	return ide_write_sectors(0, lba, 1, (const uint16_t *)buf);
}

// static int read_sectors(uint32_t lba, uint8_t count, void *buf) {
// 	return ide_read_sectors(0, lba, count, (uint16_t *)buf);
// }

// static int write_sectors(uint32_t lba, uint8_t count, const void *buf) {
// 	return ide_write_sectors(0, lba, count, (const uint16_t *)buf);
// }

int amfs_mkfs(uint32_t total_sectors) {
    if (total_sectors < 3) return -1;  /* Need superblock + dir + at least 1 data */

    memset(&sb, 0, sizeof(sb));
    memcpy(sb.magic, AMFS_MAGIC, 4);
    sb.version       = AMFS_VERSION;
    sb.total_sectors = total_sectors;
    sb.dir_sector    = 1;
    sb.data_sector   = 1 + AMFS_MAX_FILES;  /* 64 dir sectors */
    sb.max_files     = AMFS_MAX_FILES;
    sb.sector_size   = AMFS_SECTOR_SIZE;
    sb.file_count    = 0;

    /* Write superblock */
    if (write_sector(0, &sb) != 0) return -1;

    /* Clear directory table */
    uint8_t zero_sector[AMFS_SECTOR_SIZE];
    memset(zero_sector, 0, AMFS_SECTOR_SIZE);

    for (uint32_t i = 0; i < AMFS_MAX_FILES; i++) {
        if (write_sector(sb.dir_sector + i, zero_sector) != 0) return -1;
    }

    printk("[AMFS] Formatted %d sectors (%d KB)\n",
           total_sectors, (total_sectors * 512) / 1024);
    return 0;
}

int amfs_mount(void) {
	if (read_sector(0, &sb) != 0) {
		printk("[amfs] Failed to read superblock\n");
        return -1;
	}

	if (memcmp(sb.magic, AMFS_MAGIC, 4) != 0) {
        printk("[amfs] No valid filesystem found (magic mismatch)\n");
        return -1;
    }

    if (sb.version != AMFS_VERSION) {
        printk("[amfs] Version mismatch: expected %d, got %d\n",
               AMFS_VERSION, sb.version);
        return -1;
    }

    mounted = 1;
    printk("[amfs] Mounted, %d/%d files used\n", sb.file_count, sb.max_files);
    return 0;
}

static amfs_dirent_t* find_dirent(const char* name, int* index) {
    static amfs_dirent_t entry;

    for (uint32_t i = 0; i < sb.max_files; i++) {
        if (read_sector(sb.dir_sector + i, &entry) != 0) continue;

        if (name == NULL) {
            /* Looking for free entry */
            if (!entry.used) {
                if (index) *index = i;
                return &entry;
            }
        } else {
            /* Looking for named file */
            if (entry.used && strcmp(entry.name, name) == 0) {
                if (index) *index = i;
                return &entry;
            }
        }
    }
    return NULL;
}

static uint32_t alloc_data_sector(uint32_t size_in_sectors) {
	(void)size_in_sectors;
    /* Simple allocator: find first gap after existing files */
    uint32_t next_sector = sb.data_sector;

    for (uint32_t i = 0; i < sb.max_files; i++) {
        static amfs_dirent_t e;
        if (read_sector(sb.dir_sector + i, &e) != 0) continue;
        if (e.used) {
            uint32_t end = e.start_sector + ((e.size + 511) / 512);
            if (end > next_sector) next_sector = end;
        }
    }

    return next_sector;
}

int amfs_write_file(const char* name, const char* data, uint32_t size) {
    if (!mounted) return -1;
    if (!name || !data || size == 0) return -1;
    if (strlen(name) >= AMFS_NAME_LEN) return -1;

    /* Check if file exists — delete old version */
    int existing_idx;
    amfs_dirent_t* existing = find_dirent(name, &existing_idx);
    if (existing) {
        existing->used = 0;
        write_sector(sb.dir_sector + existing_idx, existing);
        sb.file_count--;
    }

    /* Find free directory entry */
    int idx;
    amfs_dirent_t* entry = find_dirent(NULL, &idx);
    if (!entry) {
        printk("[amfs] No free directory entries\n");
        return -1;
    }

    /* Allocate data space */
    uint32_t sectors_needed = (size + 511) / 512;
    uint32_t start = alloc_data_sector(sectors_needed);

    if (start + sectors_needed > sb.total_sectors) {
        printk("[amfs] Not enough space on disk\n");
        return -1;
    }

    /* Write data */
    for (uint32_t i = 0; i < sectors_needed; i++) {
        uint8_t sector[AMFS_SECTOR_SIZE];
        memset(sector, 0, AMFS_SECTOR_SIZE);
        uint32_t offset = i * AMFS_SECTOR_SIZE;
        uint32_t to_copy = size - offset;
        if (to_copy > AMFS_SECTOR_SIZE) to_copy = AMFS_SECTOR_SIZE;
        memcpy(sector, data + offset, to_copy);
        if (write_sector(start + i, sector) != 0) return -1;
    }

    /* Write directory entry */
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, name, AMFS_NAME_LEN - 1);
    entry->start_sector = start;
    entry->size = size;
    entry->used = 1;

    if (write_sector(sb.dir_sector + idx, entry) != 0) return -1;

    /* Update superblock */
    sb.file_count++;
    write_sector(0, &sb);

    printk("[amfs] Wrote '%s' (%d bytes) at sector %d\n", name, size, start);
    return 0;
}

int amfs_read_file(const char* name, char* buf, uint32_t buf_size) {
    if (!mounted) return -1;
    if (!name || !buf || buf_size == 0) return -1;

    amfs_dirent_t* entry = find_dirent(name, NULL);
    if (!entry) {
        printk("[amfs] File '%s' not found\n", name);
        return -1;
    }

    uint32_t to_read = entry->size;
    if (to_read > buf_size) to_read = buf_size;

    uint32_t sectors = (to_read + 511) / 512;
    for (uint32_t i = 0; i < sectors; i++) {
        uint8_t sector[AMFS_SECTOR_SIZE];
        if (read_sector(entry->start_sector + i, sector) != 0) return -1;

        uint32_t offset = i * AMFS_SECTOR_SIZE;
        uint32_t chunk = to_read - offset;
        if (chunk > AMFS_SECTOR_SIZE) chunk = AMFS_SECTOR_SIZE;
        memcpy(buf + offset, sector, chunk);
    }

    return to_read;
}

int amfs_delete_file(const char* name) {
    if (!mounted) return -1;

    int idx;
    amfs_dirent_t* entry = find_dirent(name, &idx);
    if (!entry) return -1;

    entry->used = 0;
    write_sector(sb.dir_sector + idx, entry);

    sb.file_count--;
    write_sector(0, &sb);

    printk("[amfs] Deleted '%s'\n", name);
    return 0;
}

void amfs_ls(void) {
    if (!mounted) {
        printk("[amfs] Not mounted\n");
        return;
    }

    printk("\n=== AmFS Contents ===\n");
    int count = 0;

    for (uint32_t i = 0; i < sb.max_files; i++) {
        static amfs_dirent_t entry;
        if (read_sector(sb.dir_sector + i, &entry) != 0) continue;

        if (entry.used) {
            printk("  %-32s %6d bytes  sector %d\n",  entry.name, entry.size, entry.start_sector);
            count++;
        }
    }

    printk("=======================\n");
    printk("Total: %d file(s)\n\n", count);
}

int amfs_exists(const char* name) {
    return find_dirent(name, NULL) != NULL;
}