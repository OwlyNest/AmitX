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

SMKFS_STATUS read_block(smkfs_mount_t *mnt, ULONGLONG block, PVOID buf) {
    if (SMKFS_BLOCK_SIZE % mnt->sb.sector_size != 0) {
        return SMKFS_ERR_IO;
    }

    CHAR sectors = SMKFS_BLOCK_SIZE / mnt->sb.sector_size;
    ULONG lba = (ULONG)(block * sectors);
    PUCHAR ptr = (PUCHAR)buf;

    for (uint8_t i = 0; i < sectors; i++) {
        if (ide_read_sectors(mnt->drive_num, lba + i, 1, (uint16_t *)(ptr + i * mnt->sb.sector_size)) != 0) {
            return SMKFS_ERR_IO;
        }
    }
    
    return SMKFS_OK;
}

SMKFS_STATUS write_block(smkfs_mount_t *mnt, ULONGLONG block, const PVOID buf) {
    if (SMKFS_BLOCK_SIZE % mnt->sb.sector_size != 0) {
        return SMKFS_ERR_IO;
    }
    
    CHAR sectors = SMKFS_BLOCK_SIZE / mnt->sb.sector_size;
    ULONG lba = (ULONG)(block * sectors);
    const PUCHAR ptr = (const PUCHAR)buf;

    for (CHAR i = 0; i < sectors; i++) {
        if (ide_write_sectors(mnt->drive_num, lba + i, 1, (const uint16_t *)(ptr + i * mnt->sb.sector_size)) != 0) {
            printk("[SmKFS] write_block FAILED: block=%llu LBA=%u\n",block, lba + i);
            return SMKFS_ERR_IO;
        }
    }
    return SMKFS_OK;
}