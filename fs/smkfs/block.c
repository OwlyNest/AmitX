/*
	* fs/smkfs/block.c - Block I/O Primitives
	* Author:   amity
	* Date:     Wed Jul 29 17:37:58 2026
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
#include <fs/smkfs.h>
#include <fs/smkfs_internal.h>
#include <hw/ide.h>
#include <screen/printk.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

int read_block(smkfs_mount_t *mnt, uint64_t block, void *buf) {
    uint8_t sectors = SMKFS_BLOCK_SIZE / SMKFS_SECTOR_SIZE;
    uint32_t lba = (uint32_t)(block * sectors);
    uint8_t *ptr = (uint8_t *)buf;

    for (uint8_t i = 0; i < sectors; i++) {
        if (ide_read_sectors(mnt->drive_num, lba + i, 1, (uint16_t *)(ptr + i * SMKFS_SECTOR_SIZE)) != 0) {
            return SMKFS_ERR_IO;
        }
    }
    return SMKFS_OK;
}

int write_block(smkfs_mount_t *mnt, uint64_t block, const void *buf) {
    uint8_t sectors = SMKFS_BLOCK_SIZE / SMKFS_SECTOR_SIZE;
    uint32_t lba = (uint32_t)(block * sectors);
    const uint8_t *ptr = (const uint8_t *)buf;

    for (uint8_t i = 0; i < sectors; i++) {
        if (ide_write_sectors(mnt->drive_num, lba + i, 1, (const uint16_t *)(ptr + i * SMKFS_SECTOR_SIZE)) != 0) {
            printk("[SmKFS] write_block FAILED: block=%llu LBA=%u\n",block, lba + i);
            return SMKFS_ERR_IO;
        }
    }
    return SMKFS_OK;
}