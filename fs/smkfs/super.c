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

int smkfs_mount(uint8_t drive, smkfs_mount_t *mnt) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    if (mnt->mounted) return SMKFS_ERR_INVAL;


    crc32c_test_vectors();

    mnt->drive_num = drive;
    if (read_block(mnt, 0, block) != 0) { // read_block only needs drive_num, safe to call
        printk("[SmKFS] Failed to read superblock\n");
        return SMKFS_ERR_IO;
    }

    memcpy(&mnt->sb, block, sizeof(mnt->sb));

    if (header_validate(&mnt->sb.header, SMKFS_ST_SUPERBLOCK) != 0) {
        printk("[SmKFS] Superblock header invalid\n");
        return SMKFS_ERR_CORRUPT;
    }

    if (header_checksum_verify(&mnt->sb.header, block, mnt->sb.header.length) != 0) {
        printk("[SmKFS] Superblock checksum mismatch\n");
        return SMKFS_ERR_CORRUPT;
    }

    mnt->sb.mount_count++;
    mnt->sb.last_mount_time = 0; /* TODO: timekeeping */
    mnt->sb.flags &= ~SMKFS_SBF_CLEAN;

    journal_replay(mnt); // grabs from SB, save 
    mnt->mounted = 1;
    printk("[SmKFS] Mounted drive %d, %llu blocks, %llu free\n", drive, mnt->sb.total_blocks, mnt->sb.free_blocks);

    smkfs_dump_superblock(mnt);

    uint8_t *root_attr = (uint8_t *)malloc(SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t));
    smkfs_record_t root_rec;
    if (root_attr && record_read(mnt, mnt->sb.root_record_id, &root_rec, root_attr, SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)) >= 0) {
        uint64_t *btree_root_ptr;
        if (record_find_attr(root_attr, SMKFS_ATTRT_DATA, (void **)&btree_root_ptr, NULL) == 0) {
            smkfs_dump_btree(mnt, *btree_root_ptr);
        } else {
            printk("[SmKFS] dump: root record has no DATA attr\n");
        }
    }

    free(root_attr);
    return SMKFS_OK;
}

int smkfs_unmount(smkfs_mount_t *mnt) {
    uint8_t block[SMKFS_BLOCK_SIZE];

    if (!mnt->mounted) return SMKFS_ERR_INVAL;

    mnt->sb.flags |= SMKFS_SBF_CLEAN;
    mnt->sb.last_mount_time = 0; /* TODO */

    memset(block, 0, sizeof(block));
    memcpy(block, &mnt->sb, sizeof(mnt->sb));
    header_checksum_update(&((smkfs_superblock_t *)block)->header, block, sizeof(smkfs_superblock_t));

    if (write_block(mnt, 0, block) != 0) {
        printk("[SmKFS] Failed to write superblock\n");
        return SMKFS_ERR_IO;
    }

    mnt->mounted = 0;
    printk("[SmKFS] Unmounted\n");
    return SMKFS_OK;
}

int smkfs_sync(smkfs_mount_t *mnt) {
    uint8_t block[SMKFS_BLOCK_SIZE];

    if (!mnt->mounted) return SMKFS_ERR_INVAL;

    memset(block, 0, sizeof(block));
    memcpy(block, &mnt->sb, sizeof(mnt->sb));
    header_checksum_update(&((smkfs_superblock_t *)block)->header, block, sizeof(smkfs_superblock_t));

    if (write_block(mnt, 0, block) != 0) {
        printk("[SmKFS] Sync failed\n");
        return SMKFS_ERR_IO;
    }

    printk("[SmKFS] Synced\n");
    return SMKFS_OK;
}

int smkfs_mkfs(uint8_t drive, uint64_t total_blocks) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_mount_t mnt;
    memset(&mnt, 0, sizeof(smkfs_mount_t));
    mnt.drive_num = drive;
    uint64_t bitmap_blocks;
    uint64_t journal_blocks;
    uint64_t data_blocks;
    uint64_t root_block;
    uint64_t btree_root;
    smkfs_superblock_t new_sb;
    smkfs_record_t root_rec;
    smkfs_btree_node_t *node;

    /*
     * MKFS writes essential data to disk. It needs write_block (196, 202, 218, 244), and FSCK (248) uses the drive number, recording line numbers wasn't the smartest idea.
    */

    if (total_blocks < 16) return SMKFS_ERR_INVAL;

    bitmap_blocks = (total_blocks + SMKFS_BLOCK_SIZE * 8 - 1) / (SMKFS_BLOCK_SIZE * 8);
    if (bitmap_blocks < 1) bitmap_blocks = 1;

    journal_blocks = 4;
    if (journal_blocks >= total_blocks - 1 - bitmap_blocks) {
        journal_blocks = total_blocks - 1 - bitmap_blocks - 1;
    }

    data_blocks = total_blocks - 1 - bitmap_blocks - journal_blocks;

    memset(&new_sb, 0, sizeof(new_sb));
    header_init(&new_sb.header, SMKFS_ST_SUPERBLOCK, sizeof(smkfs_superblock_t), 0);

    new_sb.total_blocks = total_blocks;
    new_sb.free_blocks = data_blocks - 1;
    new_sb.record_count = 1;
    new_sb.next_record_id = 1;
    new_sb.mrt_start = 0;
    new_sb.mrt_length = 0;
    new_sb.mrt_capacity = 0;
    new_sb.mrt_free_count = 0;
    new_sb.root_record_id = 1 + bitmap_blocks + journal_blocks;
    new_sb.journal_start = 1;
    new_sb.journal_length = journal_blocks;
    new_sb.journal_head = 0;
    new_sb.journal_tail = 0;
    new_sb.journal_sequence = 1;
    new_sb.bitmap_start = 1 + journal_blocks;
    new_sb.bitmap_length = bitmap_blocks;
    new_sb.alloc_meta_start = 0;
    new_sb.alloc_meta_length = 0;
    new_sb.data_start = 1 + bitmap_blocks + journal_blocks;
    new_sb.block_size = SMKFS_BLOCK_SIZE;
    new_sb.flags = SMKFS_SBF_CLEAN;
    memset(new_sb.uuid, 0, 16);
    memset(new_sb.volume_name, 0, 64);
    new_sb.creation_time = 0;      /* TODO: timekeeping */
    new_sb.last_mount_time = 0;
    new_sb.mount_count = 0;
    new_sb.max_mount_count = 0;

    memset(block, 0, sizeof(block));
    memcpy(block, &new_sb, sizeof(new_sb));
    header_checksum_update(&((smkfs_superblock_t *)block)->header, block, sizeof(smkfs_superblock_t));
    if (write_block(&mnt, 0, block) != 0) return SMKFS_ERR_IO;

    memset(block, 0, sizeof(block));
    for (uint64_t i = 0; i < journal_blocks; i++) {
        if (write_block(&mnt, 1 + i, block) != 0) return SMKFS_ERR_IO;
    }

    memset(block, 0, sizeof(block));
    block[0] = 0x03;
    for (uint64_t i = 0; i < bitmap_blocks; i++) {
        if (write_block(&mnt, 1 + journal_blocks + i, block) != 0) {
            return SMKFS_ERR_IO;
        }
    }

    root_block = new_sb.root_record_id;
    btree_root = root_block + 1;

    memset(block, 0, sizeof(block));
    node = (smkfs_btree_node_t *)block;
    header_init(&node->header, SMKFS_ST_BTREE_NODE, sizeof(smkfs_btree_node_t), SMKFS_BTN_LEAF | SMKFS_BTN_ROOT);
    node->parent_block = 0;
    node->flags = SMKFS_BTN_LEAF | SMKFS_BTN_ROOT; // Do NOT remove/edit this line, will cause nightmares
    node->key_count = 0;
    node->right_sibling = 0;
    header_checksum_update(&node->header, block, sizeof(smkfs_btree_node_t));
    if (write_block(&mnt, btree_root, block) != 0) return SMKFS_ERR_IO;

    memset(block, 0, sizeof(block));
    header_init(&root_rec.header, SMKFS_ST_RECORD, sizeof(smkfs_record_t) + 2 * sizeof(smkfs_attr_header_t) + sizeof(uint64_t), 0);
    root_rec.record_id = root_block;
    root_rec.object_type = SMKFS_ROT_DIR;
    root_rec.attr_count = 2;
    root_rec.link_count = 1;       /* Root directory has one link */
    root_rec.generation = 0;

    memcpy(block, &root_rec, sizeof(root_rec));

    smkfs_attr_header_t *name_ah = (smkfs_attr_header_t *)(block + sizeof(smkfs_record_t));
    name_ah->type = SMKFS_ATTRT_DATA;
    name_ah->flags = 0;
    name_ah->id = 0;
    name_ah->length = sizeof(uint64_t);
    memcpy(block + sizeof(smkfs_record_t) + sizeof(smkfs_attr_header_t), &btree_root, sizeof(btree_root));

    smkfs_attr_header_t *end_ah = (smkfs_attr_header_t *)(block + sizeof(smkfs_record_t) + sizeof(smkfs_attr_header_t) + sizeof(uint64_t));
    end_ah->type = SMKFS_ATTRT_END;
    end_ah->flags = 0;
    end_ah->id = 0;
    end_ah->length = 0;

    header_checksum_update(&((smkfs_record_t *)block)->header, block, root_rec.header.length);
    if (write_block(&mnt, root_block, block) != 0) return SMKFS_ERR_IO;

    printk("[SmKFS] Formatted drive %d, %llu blocks total, %llu data blocks\n", drive, total_blocks, data_blocks);

    smkfs_fsck(mnt.drive_num);
    return SMKFS_OK;
}

int smkfs_fsck(uint8_t drive) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_superblock_t check_sb;
    int errors = 0;
    smkfs_mount_t mnt;
    memset(&mnt, 0, sizeof(smkfs_mount_t));
    mnt.drive_num = drive;

    if (read_block(&mnt, 0, block) != 0) {
        printk("[SmKFS] fsck: Cannot read superblock\n");
        return SMKFS_ERR_IO;
    }

    memcpy(&check_sb, block, sizeof(check_sb));
    mnt.sb = check_sb; // bitmap_test reads sb.data_start, sb.bitmap_start and drive_num (trough read_block)
    
    /*
    * FSCK uses read_block, (256, 298) which needs the drive number for the IDE device.
    * Secondly it uses bitmap_test (289) which needs the superblock. check_sb is a direct copy of block 0, so that's (should be) identical to the superblock in the mount context.
    */

    if (header_validate(&check_sb.header, SMKFS_ST_SUPERBLOCK) != 0) {
        printk("[SmKFS] fsck: Superblock header invalid\n");
        return SMKFS_ERR_CORRUPT;
    }

    if (header_checksum_verify(&check_sb.header, block, check_sb.header.length) != 0) {
        printk("[SmKFS] fsck: Superblock checksum mismatch\n");
        errors++;
    }

    if (check_sb.total_blocks == 0 || check_sb.data_start >= check_sb.total_blocks) {
        printk("[SmKFS] fsck: Invalid layout\n");
        return SMKFS_ERR_CORRUPT;
    }

    if (check_sb.free_blocks > check_sb.total_blocks - check_sb.data_start) {
        printk("[SmKFS] fsck: free_blocks mismatch, fixing\n");
        check_sb.free_blocks = check_sb.total_blocks - check_sb.data_start;
        errors++;
    }

    if (bitmap_test(&mnt, check_sb.root_record_id) != 1) {
        printk("[SmKFS] fsck: root_record %llu not marked allocated\n", check_sb.root_record_id);
        errors++;
    }

    if (errors > 0) {
        memset(block, 0, sizeof(block));
        memcpy(block, &check_sb, sizeof(check_sb));
        header_checksum_update(&((smkfs_superblock_t *)block)->header, block, sizeof(smkfs_superblock_t));
        write_block(&mnt, 0, block);
        printk("[SmKFS] fsck: Repaired %d errors\n", errors);
        return 1;
    }

    printk("[SmKFS] fsck: Clean\n");
    return SMKFS_OK;
}