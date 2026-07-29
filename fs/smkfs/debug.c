/*
	* fs/smkfs/debug.c - Debug and Diagnostic Output
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

int smkfs_dump_superblock(void) {
    if (!mounted) {
        printk("[SmKFS] Not mounted\n");
        return SMKFS_ERR_INVAL;
    }

    printk("\n=== SmKFS Superblock ===\n");
    printk("Magic:        %.4s\n", sb.header.magic);
    printk("Version:      %u\n", sb.header.version);
    printk("Type:         0x%04X\n", sb.header.type);
    printk("Length:       %u\n", sb.header.length);
    printk("Flags:        0x%08X\n", sb.header.flags);
    printk("Checksum:     0x%08X\n", sb.header.checksum);
    printk("Total blocks: %llu\n", sb.total_blocks);
    printk("Free blocks:  %llu\n", sb.free_blocks);
    printk("Records:      %llu\n", sb.record_count);
    printk("Next ID:      %llu\n", sb.next_record_id);
    printk("Root record:  %llu\n", sb.root_record);
    printk("Journal:      %llu + %llu\n", sb.journal_start, sb.journal_length);
    printk("Bitmap:       %llu + %llu\n", sb.bitmap_start, sb.bitmap_length);
    printk("Data start:   %llu\n", sb.data_start);
    printk("Block size:   %u\n", sb.block_size);
    printk("========================\n\n");
    return SMKFS_OK;
}

int smkfs_dump_record(uint64_t record_id) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_record_t *rec;
    const uint8_t *ptr;

    if (!mounted || record_id == 0) return SMKFS_ERR_INVAL;
    if (read_block(record_id, block) != 0) {
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

    ptr = block + sizeof(smkfs_record_t);
    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        if (ah->type == SMKFS_ATTRT_END) {
            printk("  [END]\n");
            break;
        }
		
        printk("  Attr %s (0x%04X), len %u: ", smkfs_attr_name(ah->type), ah->type, ah->length);
        smkfs_attr_debug_print(ah->type, ptr + sizeof(smkfs_attr_header_t), ah->length);
        printk("\n");
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }

    printk("==================\n\n");
    return SMKFS_OK;
}

int smkfs_dump_journal(void) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_journal_entry_t *ent;

    if (!mounted) {
        printk("[SmKFS] Not mounted\n");
        return SMKFS_ERR_INVAL;
    }

    printk("\n=== Journal ===\n");
    for (uint64_t i = 0; i < sb.journal_length; i++) {
        if (read_block(sb.journal_start + i, block) != 0) continue;

        ent = (smkfs_journal_entry_t *)block;
        if (header_validate(&ent->header, SMKFS_ST_JOURNAL_ENT) != 0) {
            continue;
		}

        printk("Entry %llu: seq=%llu, op=%u, block=%llu, len=%u\n", i, ent->sequence, ent->operation, ent->target_block, ent->data_length);

        if (ent->operation == SMKFS_JOP_COMMIT) {
            printk("  [COMMIT]\n");
        } else if (ent->operation == SMKFS_JOP_WRITE) {
            printk("  [WRITE %llu bytes]\n", ent->data_length);
        } else if (ent->operation == SMKFS_JOP_ALLOC) {
            printk("  [ALLOC %u blocks]\n", ent->data_length);
        } else if (ent->operation == SMKFS_JOP_FREE) {
            printk("  [FREE %u blocks]\n", ent->data_length);
        }
    }

    printk("===============\n\n");
    return SMKFS_OK;
}

int smkfs_dump_btree(uint64_t root_block) {
    if (!mounted) {
        printk("[SmKFS] Not mounted\n");
        return SMKFS_ERR_INVAL;
    }
	
    printk("\n=== B+ Tree ===\n");
    btree_dump_recursive(root_block, 0);
    printk("===============\n\n");
    return SMKFS_OK;
}