/*
	* fs/smkfs/super.c - Superblock and Volume Lifecycle (G1)
	* Author:   amity
	* Date:     Wed Jul 29 17:38:48 2026
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
#include <mm/heap.h>
#include <lib/string.h>
#include <screen/printk.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

SMKFS_STATUS smkfs_mount(UCHAR drive, _SMKFS_MOUNT *mnt) {
    UCHAR block[SMKFS_BLOCK_SIZE];

    if (mnt->mounted) return SMKFS_ERR_INVAL;

    crc32c_test_vectors();

    mnt->drive_num = drive;
    mnt->sb.sector_size = 512; /* will be replaced by actual superblock read,
                                  reading the superblock itself will fail on a 0-division error otherwise. */
    block_cache_init(mnt);
    if (read_block(mnt, 0, block) != 0) { // read_block only needs drive_num, safe to call
        printk("[SmKFS] Failed to read superblock\n");
        bitmap_shutdown(mnt);
        block_cache_shutdown(mnt);
        return SMKFS_ERR_IO;
    }

    memcpy(&mnt->sb, block, sizeof(mnt->sb));

    if (header_validate(&mnt->sb.header, SMKFS_ST_SUPERBLOCK) != SMKFS_OK) {
        printk("[SmKFS] Superblock header invalid\n");
        bitmap_shutdown(mnt);
        block_cache_shutdown(mnt);
        return SMKFS_ERR_CORRUPT;
    }

    if (header_checksum_verify(&mnt->sb.header, block, mnt->sb.header.length) != SMKFS_OK) {
        printk("[SmKFS] Superblock checksum mismatch\n");
        bitmap_shutdown(mnt);
        block_cache_shutdown(mnt);
        return SMKFS_ERR_CORRUPT;
    }

    journal_replay(mnt);
    if (bitmap_init_regions(mnt) != SMKFS_OK) {
        printk("[SMKFS] Failed to init region cache\n");
        bitmap_shutdown(mnt);
        block_cache_shutdown(mnt);
        return SMKFS_ERR_NOMEM;
    }

    mnt->sb.mount_count++;
    mnt->sb.last_mount_time = 0;
    mnt->sb.flags &= ~SMKFS_SBF_CLEAN;
    mnt->mounted = 1;
    printk("[SmKFS] Mounted drive %d, %llu blocks, %llu free\n", drive, mnt->sb.total_blocks, mnt->sb.free_blocks);

    smkfs_dump_superblock(mnt);

    PUCHAR root_attr = (PUCHAR)malloc(SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD));
    _SMKFS_RECORD root_rec;
    if (root_attr && record_read(mnt, mnt->sb.root_record_id, &root_rec, root_attr, SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)) >= 0) {
        ULONGLONG *btree_root_ptr;
        if (record_find_attr(root_attr, SMKFS_ATTRT_DATA, (PVOID *)&btree_root_ptr, NULL) == SMKFS_OK) {
            smkfs_dump_btree(mnt, *btree_root_ptr);
        } else {
            printk("[SmKFS] dump: root record has no DATA attr\n");
        }
    }

    free(root_attr);
    return SMKFS_OK;
}

SMKFS_STATUS smkfs_unmount(_SMKFS_MOUNT *mnt) {
    UCHAR block[SMKFS_BLOCK_SIZE];

    if (!mnt->mounted) return SMKFS_ERR_INVAL;

    /* 1. Flush all dirty cache blocks first */
    if (block_cache_flush(mnt) != SMKFS_OK) {
        printk("[SmKFS] Unmount: cache flush failed\n");
        mnt->mounted = 0;
        bitmap_shutdown(mnt);
        block_cache_shutdown(mnt);
        return SMKFS_ERR_IO;
    }

    /* 2. Now mark clean and write superblock */
    mnt->sb.flags |= SMKFS_SBF_CLEAN;
    mnt->sb.last_mount_time = 0;

    memset(block, 0, sizeof(block));
    memcpy(block, &mnt->sb, sizeof(mnt->sb));
    header_checksum_update(&((_SMKFS_SUPERBLOCK *)block)->header, block, sizeof(_SMKFS_SUPERBLOCK));

    if (write_block(mnt, 0, block) != SMKFS_OK) {
        printk("[SmKFS] Failed to write superblock\n");
        mnt->mounted = 0;
        bitmap_shutdown(mnt);
        block_cache_shutdown(mnt);
        return SMKFS_ERR_IO;
    }

    bitmap_shutdown(mnt);
    block_cache_shutdown(mnt);
    mnt->mounted = 0;
    printk("[SmKFS] Unmounted\n");
    return SMKFS_OK;
}

SMKFS_STATUS smkfs_sync(_SMKFS_MOUNT *mnt) {
    UCHAR block[SMKFS_BLOCK_SIZE];

    if (!mnt->mounted) return SMKFS_ERR_INVAL;

    /* Flush dirty blocks before writing the superblock */
    if (block_cache_flush(mnt) != SMKFS_OK) {
        printk("[SmKFS] Sync: cache flush failed\n");
        return SMKFS_ERR_IO;
    }

    memset(block, 0, sizeof(block));
    memcpy(block, &mnt->sb, sizeof(mnt->sb));
    header_checksum_update(&((_SMKFS_SUPERBLOCK *)block)->header, block, sizeof(_SMKFS_SUPERBLOCK));

    if (write_block(mnt, 0, block) != SMKFS_OK) {
        printk("[SmKFS] Sync failed\n");
        return SMKFS_ERR_IO;
    }

    printk("[SmKFS] Synced\n");
    return SMKFS_OK;
}

SMKFS_STATUS smkfs_mkfs(UCHAR drive, ULONGLONG total_blocks, ULONGLONG sector_size) {
    UCHAR block[SMKFS_BLOCK_SIZE];
    _SMKFS_MOUNT *mnt = malloc(sizeof(_SMKFS_MOUNT));
    if (!mnt) {
        printk("[MKFS] Failed to allocate mount context\n");
        return -1;
    }
    memset(mnt, 0, sizeof(_SMKFS_MOUNT));
    mnt->drive_num = drive;
    block_cache_init(mnt);

    ULONGLONG bitmap_blocks;
    ULONGLONG journal_blocks;
    SMKFS_BLOCK mrt_start;
    ULONGLONG mrt_blocks;
    SMKFS_BLOCK data_start;
    ULONGLONG data_blocks;
    SMKFS_BLOCK btree_root;
    SMKFS_RECORD_ID root_id;
    _SMKFS_BTREE_NODE *node;
    
    if (total_blocks < 16) {
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_INVAL;
    }

    bitmap_blocks = (total_blocks + SMKFS_BLOCK_SIZE * 8 - 1) / (SMKFS_BLOCK_SIZE * 8);
    if (bitmap_blocks < 1) bitmap_blocks = 1;

    journal_blocks = 4;
    if (journal_blocks >= total_blocks - 1 - bitmap_blocks) {
        journal_blocks = total_blocks - 1 - bitmap_blocks - 1;
    }

    mrt_blocks = (total_blocks + (SMKFS_BLOCK_SIZE / sizeof(_SMKFS_MRT_ENTRY)) - 1) / (SMKFS_BLOCK_SIZE / sizeof(_SMKFS_MRT_ENTRY));
    if (mrt_blocks < 1) mrt_blocks = 1;
    mrt_start = 1 + journal_blocks + bitmap_blocks;

    data_start = 1 + bitmap_blocks + journal_blocks + mrt_blocks;
    data_blocks = total_blocks - 1 - bitmap_blocks - journal_blocks - mrt_blocks;

    /* --- Populate mnt.sb with layout fields, before anything is allocated --- */
    header_init(&mnt->sb.header, SMKFS_ST_SUPERBLOCK, sizeof(_SMKFS_SUPERBLOCK), 0);
    mnt->sb.total_blocks = total_blocks;
    mnt->sb.free_blocks = data_blocks;
    mnt->sb.sector_size = sector_size;
    mnt->sb.record_count = 0;           /* record_alloc will bump this */
    mnt->sb.next_record_id = 1;
    mnt->sb.root_record_id = 0;         /* filled in once root is allocated */
    mnt->sb.journal_start = 1;
    mnt->sb.journal_length = journal_blocks;
    mnt->sb.journal_head = 0;
    mnt->sb.journal_tail = 0;
    mnt->sb.journal_sequence = 1;
    mnt->sb.bitmap_start = 1 + journal_blocks;
    mnt->sb.bitmap_length = bitmap_blocks;
    mnt->sb.alloc_meta_start = 0;
    mnt->sb.alloc_meta_length = 0;
    mnt->sb.data_start = data_start;
    mnt->sb.block_size = SMKFS_BLOCK_SIZE;
    mnt->sb.flags = SMKFS_SBF_CLEAN;
    memset(mnt->sb.uuid, 0, 16);
    memset(mnt->sb.volume_name, 0, 64);
    mnt->sb.creation_time = 0;          /* TODO: timekeeping */
    mnt->sb.last_mount_time = 0;
    mnt->sb.mount_count = 0;
    mnt->sb.max_mount_count = 0;

    if (mrt_init(mnt, mrt_start, mrt_blocks) != SMKFS_OK) {
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_INVAL;
    }

    mnt->sb.mrt_free_count = mnt->sb.mrt_capacity - 1;   /* slot 0 reserved */

    /* --- Zero journal region --- */
    memset(block, 0, sizeof(block));
    for (uint64_t i = 0; i < journal_blocks; i++) {
        if (write_block(mnt, 1 + i, block) != 0) {
            block_cache_shutdown(mnt);
            free(mnt);
            return SMKFS_ERR_IO;
        }
    }

    /* --- Zero bitmap region; nothing is pre-marked, everything below
     *     allocates through bitmap_alloc like normal operation would --- */
    memset(block, 0, sizeof(block));
    for (uint64_t i = 0; i < bitmap_blocks; i++) {
        if (write_block(mnt, mnt->sb.bitmap_start + i, block) != 0) {
            block_cache_shutdown(mnt);
            free(mnt);
            return SMKFS_ERR_IO;
        }
    }

    /* --- Format MRT region --- */
    if (mrt_format(mnt, mrt_start, mrt_blocks) != SMKFS_OK) {
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_IO;
    }

    /* --- Allocate the root B+ tree node. Not a record, same convention
     *     btree_insert already uses for new leaves: bitmap_alloc. --- */
    btree_root = bitmap_alloc(mnt);
    if (btree_root == 0) {
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_NOSPC;
    }

    memset(block, 0, sizeof(block));
    node = (_SMKFS_BTREE_NODE *)block;
    header_init(&node->header, SMKFS_ST_BTREE_NODE, sizeof(_SMKFS_BTREE_NODE), SMKFS_BTN_LEAF | SMKFS_BTN_ROOT);
    node->parent_block = 0;
    node->flags = SMKFS_BTN_LEAF | SMKFS_BTN_ROOT; // Do NOT remove/edit this line, will cause nightmares
    node->key_count = 0;
    node->right_sibling = 0;
    header_checksum_update(&node->header, block, sizeof(_SMKFS_BTREE_NODE));
    if (write_block(mnt, btree_root, block) != SMKFS_OK) {
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_IO;
    }

    /* --- Allocate the root directory record through the same two-phase
     *     MRT/bitmap path every other record uses. --- */
    root_id = record_alloc(mnt, SMKFS_ROT_DIR);
    if (root_id == 0) {
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_NOSPC;
    }

    {
        UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
        _SMKFS_RECORD root_rec;

        if (record_read(mnt, root_id, &root_rec, attr_buf, sizeof(attr_buf)) < 0) {
            block_cache_shutdown(mnt);
            free(mnt);
            return SMKFS_ERR_IO;
        }

        if (record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_DATA, &btree_root, sizeof(btree_root)) != SMKFS_OK) {
            block_cache_shutdown(mnt);
            free(mnt);
            return SMKFS_ERR_NOSPC;
        }

        root_rec.attr_count++;
        root_rec.header.length = sizeof(_SMKFS_RECORD) + attr_buf_total_len(attr_buf);

        if (record_write(mnt, root_id, &root_rec, attr_buf) != SMKFS_OK) {
            block_cache_shutdown(mnt);
            free(mnt);
            return SMKFS_ERR_IO;
        }
    }

    mnt->sb.root_record_id = root_id;

    /* --- Now that every field reflects reality, persist the superblock --- */
    memset(block, 0, sizeof(block));
    memcpy(block, &mnt->sb, sizeof(mnt->sb));
    header_checksum_update(&((_SMKFS_SUPERBLOCK *)block)->header, block, sizeof(_SMKFS_SUPERBLOCK));
    if (write_block(mnt, 0, block) != 0) {
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_IO;
    }

    printk("[SmKFS] Formatted drive %d, %llu blocks total, %llu data blocks\n", drive, total_blocks, data_blocks);

    block_cache_flush(mnt);
    smkfs_fsck(mnt->drive_num);
    block_cache_shutdown(mnt);
    free(mnt);
    return SMKFS_OK;
}

SMKFS_STATUS smkfs_fsck(UCHAR drive) {
    UCHAR block[SMKFS_BLOCK_SIZE];
    _SMKFS_SUPERBLOCK check_sb;
    LONG errors = 0;
    _SMKFS_MOUNT *mnt = malloc(sizeof(_SMKFS_MOUNT));
    if (!mnt) {
        printk("[storage] Failed to allocate mount context\n");
        goto cleanup;
    }

    memset(mnt, 0, sizeof(_SMKFS_MOUNT));
    mnt->drive_num = drive;
    mnt->sb.sector_size = 512; /* Same as in mount */
    block_cache_init(mnt);
    

    if (read_block(mnt, 0, block) != SMKFS_OK) {
        printk("[SmKFS] fsck: Cannot read superblock\n");
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_IO;
    }

    memcpy(&check_sb, block, sizeof(check_sb));
    mnt->sb = check_sb;

    /*
     * FSCK uses read_block, (256, 298) which needs the drive number for the IDE device.
     * Secondly it uses bitmap_test (289) which needs the superblock. check_sb is a direct copy of block 0, so that's (should be) identical to the superblock in the mount context.
     * I edited this file, meaning the line numbers no longer work, sorry!
    */

    if (header_validate(&check_sb.header, SMKFS_ST_SUPERBLOCK) != SMKFS_OK) {
        printk("[SmKFS] fsck: Superblock header invalid\n");
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_CORRUPT;
    }

    if (header_checksum_verify(&check_sb.header, block, check_sb.header.length) != SMKFS_OK) {
        printk("[SmKFS] fsck: Superblock checksum mismatch\n");
        errors++;
    }

    if (check_sb.total_blocks == 0 || check_sb.data_start >= check_sb.total_blocks) {
        printk("[SmKFS] fsck: Invalid layout\n");
        block_cache_shutdown(mnt);
        free(mnt);
        return SMKFS_ERR_CORRUPT;
    }

    if (check_sb.free_blocks > check_sb.total_blocks - check_sb.data_start) {
        printk("[SmKFS] fsck: free_blocks mismatch, fixing\n");
        check_sb.free_blocks = check_sb.total_blocks - check_sb.data_start;
        errors++;
    }

    SMKFS_BLOCK phys_block;
    SMKFS_STATUS mrt_ret = mrt_resolve(mnt, check_sb.root_record_id, &phys_block, NULL, NULL);
    if (mrt_ret != SMKFS_OK) {
        block_cache_shutdown(mnt);
        free(mnt);
        return mrt_ret;
    }

    if (bitmap_test(mnt, phys_block) != 1) {
        printk("[SmKFS] fsck: root_record %llu not marked allocated\n", check_sb.root_record_id);
        errors++;
    }

    if (errors > 0) {
        memset(block, 0, sizeof(block));
        memcpy(block, &check_sb, sizeof(check_sb));
        header_checksum_update(&((_SMKFS_SUPERBLOCK *)block)->header, block, sizeof(_SMKFS_SUPERBLOCK));
        write_block(mnt, 0, block);
        printk("[SmKFS] fsck: Repaired %d errors\n", errors);
        goto cleanup;
    }

    printk("[SmKFS] fsck: Clean\n");
    block_cache_shutdown(mnt);
    free(mnt);
    return SMKFS_OK;
cleanup:
    block_cache_shutdown(mnt);
    free(mnt);
    return 1;
}