/*
	* fs/smkfs/journal.c - Metadata Journaling (G1)
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

/* --- Prototypes ---*/

/* --- Functions ---*/

SMKFS_STATUS journal_start_transaction(_SMKFS_MOUNT *mnt) {
    if (mnt->journal_in_transaction) return SMKFS_ERR_JOURNAL;
    mnt->journal_in_transaction = 1;
    return SMKFS_OK;
}

SMKFS_STATUS journal_log_write(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PCVOID old_data, PCVOID new_data, SIZE_T len) {
    PUCHAR buf;
    _SMKFS_JOURNAL_ENTRY *ent;
    SIZE_T total_len;
    SMKFS_STATUS ret;

    if (!mnt->journal_in_transaction || len > SMKFS_BLOCK_SIZE) {
        return SMKFS_ERR_JOURNAL;
    }

    if (len > 0 && (!old_data || !new_data)) {
        return SMKFS_ERR_INVAL;
    }

    total_len = sizeof(_SMKFS_JOURNAL_ENTRY) + len;
    if (total_len > SMKFS_BLOCK_SIZE) return SMKFS_ERR_TOO_BIG;

    buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return SMKFS_ERR_NOMEM;

    memset(buf, 0, SMKFS_BLOCK_SIZE);
    ent = (_SMKFS_JOURNAL_ENTRY *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(_SMKFS_JOURNAL_ENTRY) + len, 0);
    ent->sequence = mnt->journal_next_sequence++;
    ent->target_block = block;
    ent->operation = SMKFS_JOP_WRITE;
    ent->data_length = (ULONG)len;
    ent->record_id = 0;

    if (len > 0) {
        memcpy(buf + sizeof(_SMKFS_JOURNAL_ENTRY), new_data, len);
    }

    header_checksum_update(&ent->header, buf, ent->header.length);

    ret = write_block(mnt, mnt->sb.journal_start + mnt->journal_write_pos, buf);
    free(buf);
    if (ret != SMKFS_OK) {
        printk("[SmKFS] journal_log_write FAILED at journal block %llu\n", mnt->sb.journal_start + mnt->journal_write_pos);
        return SMKFS_ERR_IO;
    }

    mnt->journal_write_pos++;
    if (mnt->journal_write_pos >= mnt->sb.journal_length) {
        mnt->journal_write_pos = 0;
    }

    return SMKFS_OK;
}

SMKFS_STATUS journal_log_alloc(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, ULONG count) {
    PUCHAR buf;
    _SMKFS_JOURNAL_ENTRY *ent;
    SMKFS_STATUS ret;

    if (!mnt->journal_in_transaction) return SMKFS_ERR_JOURNAL;

    buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return SMKFS_ERR_NOMEM;

    memset(buf, 0, SMKFS_BLOCK_SIZE);
    ent = (_SMKFS_JOURNAL_ENTRY *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(_SMKFS_JOURNAL_ENTRY), 0);
    ent->sequence = mnt->journal_next_sequence++;
    ent->target_block = block;
    ent->operation = SMKFS_JOP_ALLOC;
    ent->data_length = count;
    ent->record_id = 0;
    header_checksum_update(&ent->header, buf, ent->header.length);

    ret = write_block(mnt, mnt->sb.journal_start + mnt->journal_write_pos, buf);
    free(buf);
    if (ret != SMKFS_OK) return SMKFS_ERR_IO;

    mnt->journal_write_pos++;
    if (mnt->journal_write_pos >= mnt->sb.journal_length) mnt->journal_write_pos = 0;
    return SMKFS_OK;
}

SMKFS_STATUS journal_log_free(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, ULONG count) {
    PUCHAR buf;
    _SMKFS_JOURNAL_ENTRY *ent;
    SMKFS_STATUS ret;

    if (!mnt->journal_in_transaction) return SMKFS_ERR_JOURNAL;

    buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return SMKFS_ERR_NOMEM;

    memset(buf, 0, SMKFS_BLOCK_SIZE);
    ent = (_SMKFS_JOURNAL_ENTRY *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(_SMKFS_JOURNAL_ENTRY), 0);
    ent->sequence = mnt->journal_next_sequence++;
    ent->target_block = block;
    ent->operation = SMKFS_JOP_FREE;
    ent->data_length = count;
    ent->record_id = 0;
    header_checksum_update(&ent->header, buf, ent->header.length);

    ret = write_block(mnt, mnt->sb.journal_start + mnt->journal_write_pos, buf);
    free(buf);
    if (ret != SMKFS_OK) return SMKFS_ERR_IO;

    mnt->journal_write_pos++;
    if (mnt->journal_write_pos >= mnt->sb.journal_length) {
        mnt->journal_write_pos = 0;
    }

    return SMKFS_OK;
}

SMKFS_STATUS journal_commit(_SMKFS_MOUNT *mnt) {
    PUCHAR buf;
    _SMKFS_JOURNAL_ENTRY *ent;
    SMKFS_STATUS ret;

    if (!mnt->journal_in_transaction) return SMKFS_ERR_JOURNAL;

    buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return SMKFS_ERR_NOMEM;

    memset(buf, 0, SMKFS_BLOCK_SIZE);
    ent = (_SMKFS_JOURNAL_ENTRY *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(_SMKFS_JOURNAL_ENTRY), 0);
    ent->sequence = mnt->journal_next_sequence++;
    ent->target_block = 0;
    ent->operation = SMKFS_JOP_COMMIT;
    ent->data_length = 0;
    ent->record_id = 0;
    header_checksum_update(&ent->header, buf, ent->header.length);

    ret = write_block(mnt, mnt->sb.journal_start + mnt->journal_write_pos, buf);
    free(buf);
    if (ret != SMKFS_OK) return SMKFS_ERR_IO;

    mnt->journal_write_pos++;
    if (mnt->journal_write_pos >= mnt->sb.journal_length) {
        mnt->journal_write_pos = 0;
    }

    mnt->journal_in_transaction = 0;
    return SMKFS_OK;
}

SMKFS_STATUS journal_replay(_SMKFS_MOUNT *mnt) {
    UCHAR buf[SMKFS_BLOCK_SIZE];

    for (ULONGLONG i = 0; i < mnt->sb.journal_length; i++) {
        memset(buf, 0, sizeof(buf));
        write_block(mnt, mnt->sb.journal_start + i, buf);
    }

    mnt->journal_write_pos = 0;
    return SMKFS_OK;
}