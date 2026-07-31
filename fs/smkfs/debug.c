/*
	* fs/smkfs/debug.c - Debug and Diagnostic Output (G1)
	* Author:   amity
	* Date:     Wed Jul 29 17:39:11 2026
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
#include <screen/printk.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

int smkfs_dump_superblock(smkfs_mount_t *mnt) {
    if (!mnt->mounted) {
        printk("[SmKFS] Not mounted\n");
        return SMKFS_ERR_INVAL;
    }

    smkfs_superblock_t sb = mnt->sb; // Not gonna rewrite all that

    printk("\n=== SmKFS Superblock (G1) ===\n");
    printk("Magic:           %.4s\n", sb.header.magic);
    printk("Version:         %u\n", sb.header.version);
    printk("Type:            0x%04X\n", sb.header.type);
    printk("Length:          %u\n", sb.header.length);
    printk("Flags:           0x%08X\n", sb.header.flags);
    printk("Checksum:        0x%08X\n", sb.header.checksum);
    printk("Total blocks:    %llu\n", sb.total_blocks);
    printk("Free blocks:     %llu\n", sb.free_blocks);
    printk("Records:         %llu\n", sb.record_count);
    printk("Next ID:         %llu\n", sb.next_record_id);
    printk("Root record:     %llu\n", sb.root_record_id);
    printk("MRT:             %llu + %llu (cap %llu, free %llu)\n", sb.mrt_start, sb.mrt_length, sb.mrt_capacity, sb.mrt_free_count);
    printk("Journal:         %llu + %llu (head %llu, tail %llu, seq %llu)\n", sb.journal_start, sb.journal_length, sb.journal_head, sb.journal_tail, sb.journal_sequence);
    printk("Bitmap:          %llu + %llu\n", sb.bitmap_start, sb.bitmap_length);
    printk("Data start:      %llu\n", sb.data_start);
    printk("Block size:      %u\n", sb.block_size);
    printk("SB flags:        0x%08X\n", sb.flags);
    printk("Mount count:     %u / %u\n", sb.mount_count, sb.max_mount_count);
    printk("Volume:          %.64s\n", sb.volume_name);
    printk("===============================\n\n");
    return SMKFS_OK;
}

int smkfs_dump_record(smkfs_mount_t *mnt, uint64_t record_id) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_record_t *rec;
    const uint8_t *ptr;

    if (!mnt->mounted || record_id == 0) return SMKFS_ERR_INVAL;
    if (read_block(mnt, record_id, block) != 0) {
        printk("[SmKFS] Cannot read record %llu\n", record_id);
        return SMKFS_ERR_IO;
    }

    rec = (smkfs_record_t *)block;
    if (header_validate(&rec->header, SMKFS_ST_RECORD) != 0) {
        printk("[SmKFS] Record %llu: invalid header\n", record_id);
        return SMKFS_ERR_CORRUPT;
    }

    printk("\n=== Record %llu ===\n", record_id);
    printk("Magic:      %.4s\n", rec->header.magic);
    printk("Version:    %u\n", rec->header.version);
    printk("Type:       0x%04X\n", rec->header.type);
    printk("Length:     %u\n", rec->header.length);
    printk("Flags:      0x%08X\n", rec->header.flags);
    printk("Checksum:   0x%08X\n", rec->header.checksum);
    printk("Record ID:  %llu\n", rec->record_id);
    printk("Obj Type:   %u\n", rec->object_type);
    printk("Attr Count: %u\n", rec->attr_count);
    printk("Link Count: %u\n", rec->link_count);
    printk("Generation: %llu\n", rec->generation);

    ptr = block + sizeof(smkfs_record_t);
    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        if (ah->type == SMKFS_ATTRT_END) {
            printk("  [END]\n");
            break;
        }

        printk("  Attr %s (0x%04X, id=%u), len %u: ", smkfs_attr_name(ah->type), ah->type, ah->id, ah->length);
        smkfs_attr_debug_print(ah->type, ptr + sizeof(smkfs_attr_header_t), ah->length);
        printk("\n");
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }

    printk("==================\n\n");
    return SMKFS_OK;
}

int smkfs_dump_journal(smkfs_mount_t *mnt) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_journal_entry_t *ent;

    if (!mnt->mounted) {
        printk("[SmKFS] Not mounted\n");
        return SMKFS_ERR_INVAL;
    }

    printk("\n=== Journal ===\n");
    for (uint64_t i = 0; i < mnt->sb.journal_length; i++) {
        if (read_block(mnt, mnt->sb.journal_start + i, block) != 0) continue;

        ent = (smkfs_journal_entry_t *)block;
        if (header_validate(&ent->header, SMKFS_ST_JOURNAL_ENT) != 0) {
            continue;
        }

        printk("Entry %llu: seq=%llu, op=%u, block=%llu, len=%u, rec=%llu\n", i, ent->sequence, ent->operation, ent->target_block, ent->data_length, ent->record_id);

        /*
        * Sinatra and C should be mandatory!
        */

        if (ent->operation == SMKFS_JOP_COMMIT) {
            printk("  [COMMIT]\n");
        } else if (ent->operation == SMKFS_JOP_WRITE) {
            printk("  [WRITE %llu bytes]\n", ent->data_length);
        } else if (ent->operation == SMKFS_JOP_ALLOC) {
            printk("  [ALLOC %u blocks]\n", ent->data_length);
        } else if (ent->operation == SMKFS_JOP_FREE) {
            printk("  [FREE %u blocks]\n", ent->data_length);
        } else if (ent->operation == SMKFS_JOP_MRT_UPDATE) {
            printk("  [MRT UPDATE]\n");
        } else if (ent->operation == SMKFS_JOP_CHECKPOINT) {
            printk("  [CHECKPOINT]\n");
        }
    }

    printk("===============\n\n");
    return SMKFS_OK;
}

int smkfs_dump_btree(smkfs_mount_t *mnt, uint64_t root_block) {
    if (!mnt->mounted) {
        printk("[SmKFS] Not mounted\n");
        return SMKFS_ERR_INVAL;
    }

    printk("\n=== B+ Tree ===\n");
    btree_dump_recursive(mnt, root_block, 0);
    printk("===============\n\n");
    return SMKFS_OK;
}