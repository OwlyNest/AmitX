/*
	* fs/smkfs/journal.c - Metadata Journaling
	* Author:   amity
	* Date:     Wed Jul 29 17:38:18 2026
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
uint64_t journal_next_sequence = 1;
int journal_in_transaction = 0;
uint64_t journal_write_pos = 0;
/* --- Prototypes ---*/

/* --- Functions ---*/

int journal_start_transaction(void) {
    if (journal_in_transaction) return SMKFS_ERR_JOURNAL;
    journal_in_transaction = 1;
    return (int)journal_next_sequence;
}

int journal_log_write(uint64_t block, const void *old_data, const void *new_data, size_t len) {
    uint8_t *buf;
    smkfs_journal_entry_t *ent;
    size_t total_len;
    int ret;

    if (!journal_in_transaction || len > SMKFS_BLOCK_SIZE) {
        return SMKFS_ERR_JOURNAL;
	}
    if (len > 0 && (!old_data || !new_data)) {
        return SMKFS_ERR_INVAL;
	}

    total_len = sizeof(smkfs_journal_entry_t) + len;
    if (total_len > SMKFS_BLOCK_SIZE) return SMKFS_ERR_TOO_BIG;

    buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return SMKFS_ERR_NOMEM;

    memset(buf, 0, SMKFS_BLOCK_SIZE);
    ent = (smkfs_journal_entry_t *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(smkfs_journal_entry_t) + len, 0);
    ent->sequence = journal_next_sequence++;
    ent->target_block = block;
    ent->operation = SMKFS_JOP_WRITE;
    ent->data_length = (uint32_t)len;

    if (len > 0) {
        memcpy(buf + sizeof(smkfs_journal_entry_t), new_data, len);
    }
    header_checksum_update(&ent->header, buf, ent->header.length);

    ret = write_block(sb.journal_start + journal_write_pos, buf);
    free(buf);
    if (ret != 0) {
        printk("[SmKFS] journal_log_write FAILED at journal block %llu\n", sb.journal_start + journal_write_pos);
        return SMKFS_ERR_IO;
    }

    journal_write_pos++;
    if (journal_write_pos >= sb.journal_length) {
        journal_write_pos = 0;
    }
    return SMKFS_OK;
}

int journal_log_alloc(uint64_t block, uint32_t count) {
    uint8_t *buf;
    smkfs_journal_entry_t *ent;
    int ret;

    if (!journal_in_transaction) return SMKFS_ERR_JOURNAL;

    buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return SMKFS_ERR_NOMEM;

    memset(buf, 0, SMKFS_BLOCK_SIZE);
    ent = (smkfs_journal_entry_t *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(smkfs_journal_entry_t), 0);
    ent->sequence = journal_next_sequence++;
    ent->target_block = block;
    ent->operation = SMKFS_JOP_ALLOC;
    ent->data_length = count;
    header_checksum_update(&ent->header, buf, ent->header.length);

    ret = write_block(sb.journal_start + journal_write_pos, buf);
    free(buf);
    if (ret != 0) return SMKFS_ERR_IO;

    journal_write_pos++;
    if (journal_write_pos >= sb.journal_length) journal_write_pos = 0;
    return SMKFS_OK;
}

int journal_log_free(uint64_t block, uint32_t count) {
    uint8_t *buf;
    smkfs_journal_entry_t *ent;
    int ret;

    if (!journal_in_transaction) return SMKFS_ERR_JOURNAL;

    buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return SMKFS_ERR_NOMEM;

    memset(buf, 0, SMKFS_BLOCK_SIZE);
    ent = (smkfs_journal_entry_t *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(smkfs_journal_entry_t), 0);
    ent->sequence = journal_next_sequence++;
    ent->target_block = block;
    ent->operation = SMKFS_JOP_FREE;
    ent->data_length = count;
    header_checksum_update(&ent->header, buf, ent->header.length);

    ret = write_block(sb.journal_start + journal_write_pos, buf);
    free(buf);
    if (ret != 0) return SMKFS_ERR_IO;

    journal_write_pos++;
    if (journal_write_pos >= sb.journal_length) {
        journal_write_pos = 0;
    }
    return SMKFS_OK;
}

int journal_commit(void) {
    uint8_t *buf;
    smkfs_journal_entry_t *ent;
    int ret;

    if (!journal_in_transaction) return SMKFS_ERR_JOURNAL;

    buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return SMKFS_ERR_NOMEM;

    memset(buf, 0, SMKFS_BLOCK_SIZE);
    ent = (smkfs_journal_entry_t *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(smkfs_journal_entry_t), 0);
    ent->sequence = journal_next_sequence++;
    ent->target_block = 0;
    ent->operation = SMKFS_JOP_COMMIT;
    ent->data_length = 0;
    header_checksum_update(&ent->header, buf, ent->header.length);

    ret = write_block(sb.journal_start + journal_write_pos, buf);
    free(buf);
    if (ret != 0) return SMKFS_ERR_IO;

    journal_write_pos++;
    if (journal_write_pos >= sb.journal_length) {
        journal_write_pos = 0;
    }

    journal_in_transaction = 0;
    return SMKFS_OK;
}

int journal_replay(void) {
    // G0 journal is broken for recovery. Clear it to prevent data loss.
    // Full fix in G1 with redo-only logging.
    uint8_t buf[SMKFS_BLOCK_SIZE];
    for (uint64_t i = 0; i < sb.journal_length; i++) {
        memset(buf, 0, sizeof(buf));
        write_block(sb.journal_start + i, buf);
    }
    journal_write_pos = 0;
    return 0;
}