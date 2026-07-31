/*
	* fs/smkfs/record.c - Record Operations (G1)
	* Author:   amity
	* Date:     Wed Jul 29 17:38:25 2026
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

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

int record_read(smkfs_mount_t *mnt, uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size) {
    uint8_t *block;
    size_t attr_len;
    int ret;

    if (!rec || !attr_buf) return SMKFS_ERR_INVAL;

    block = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!block) return SMKFS_ERR_NOMEM;

    if (read_block(mnt, record_id, block) != 0) {
        free(block);
        return SMKFS_ERR_IO;
    }

    memcpy(rec, block, sizeof(smkfs_record_t));

    if (header_validate(&rec->header, SMKFS_ST_RECORD) != 0) {
        free(block);
        return SMKFS_ERR_CORRUPT;
    }

    if (rec->header.length < sizeof(smkfs_record_t)) {
        free(block);
        return SMKFS_ERR_CORRUPT;
    }

    if (header_checksum_verify(&rec->header, block, rec->header.length) != 0) {
        free(block);
        return SMKFS_ERR_CORRUPT;
    }

    attr_len = rec->header.length - sizeof(smkfs_record_t);
    if (attr_len > buf_size) attr_len = buf_size;

    memcpy(attr_buf, block + sizeof(smkfs_record_t), attr_len);
    ret = (int)attr_len;
    free(block);
    return ret;
}

int record_write(smkfs_mount_t *mnt, uint64_t record_id, const smkfs_record_t *rec, const void *attr_buf) {
    uint8_t *block, *old_block;
    size_t total_len;
    size_t attr_len;
    int ret;
    const uint8_t *ptr;
    smkfs_record_t *wrec;

    if (!rec) return SMKFS_ERR_INVAL;
    if (sizeof(smkfs_record_t) > SMKFS_BLOCK_SIZE) {
        return SMKFS_ERR_TOO_BIG;
    }

    if (!attr_buf) return SMKFS_ERR_INVAL;

    attr_len = 0;
    ptr = (const uint8_t *)attr_buf;
    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)((uintptr_t)ptr);
        if (ah->type == SMKFS_ATTRT_END) {
            attr_len = (size_t)(ptr - (const uint8_t *)attr_buf) + sizeof(smkfs_attr_header_t);
            break;
        }

        if (attr_len > SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)) {
            return SMKFS_ERR_TOO_BIG;
        }

        ptr += sizeof(smkfs_attr_header_t) + ah->length;
        if ((size_t)(ptr - (const uint8_t *)attr_buf) > SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)) {
            return SMKFS_ERR_TOO_BIG;
        }
    }

    total_len = sizeof(smkfs_record_t) + attr_len;
    if (total_len < sizeof(smkfs_record_t) || total_len > SMKFS_BLOCK_SIZE) {
        return SMKFS_ERR_TOO_BIG;
    }

    block = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    old_block = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!block || !old_block) {
        free(block);
        free(old_block);
        return SMKFS_ERR_NOMEM;
    }

    if (read_block(mnt, record_id, old_block) != 0) {
        memset(old_block, 0, SMKFS_BLOCK_SIZE);
    }

    memset(block, 0, SMKFS_BLOCK_SIZE);
    wrec = (smkfs_record_t *)block;
    memcpy(wrec, rec, sizeof(smkfs_record_t));
    wrec->header.length = total_len;
    memcpy(block + sizeof(smkfs_record_t), attr_buf, attr_len);
    header_checksum_update(&wrec->header, block, total_len);

    journal_log_write(mnt, record_id, old_block, block, total_len);

    ret = write_block(mnt, record_id, block);
    free(block);
    free(old_block);
    return (ret == 0) ? SMKFS_OK : SMKFS_ERR_IO;
}

uint64_t record_alloc(smkfs_mount_t *mnt, uint16_t object_type) {
    uint64_t block = bitmap_alloc(mnt);
    if (block == 0) return 0;

    smkfs_record_t rec;
    header_init(&rec.header, SMKFS_ST_RECORD, sizeof(smkfs_record_t) + sizeof(smkfs_attr_header_t), 0);
    rec.record_id = block;     /* G0 compat: logical ID == physical block */
    rec.object_type = object_type;
    rec.attr_count = 0;
    rec.link_count = 1;        /* Every new record starts with one link */
    rec.generation = 0;        /* Set by MRT in G1.5 */

    smkfs_attr_header_t term;
    term.type = SMKFS_ATTRT_END;
    term.flags = 0;
    term.id = 0;
    term.length = 0;
    record_write(mnt, block, &rec, &term);

    mnt->sb.record_count++;
    return block;
}

void record_free(smkfs_mount_t *mnt, uint64_t record_id) {
    uint8_t *block;

    extent_remove_all(mnt, record_id);

    block = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (block) {
        if (read_block(mnt, record_id, block) == 0) {
            smkfs_record_t *rec = (smkfs_record_t *)block;
            rec->header.flags |= SMKFS_FLA_DELETED;
            header_checksum_update(&rec->header, block, rec->header.length);
            write_block(mnt, record_id, block);
        }

        free(block);
    }

    bitmap_clear(mnt, record_id);
    mnt->sb.record_count--;
}

size_t attr_buf_total_len(const void *attr_buf) {
    const uint8_t *ptr = (const uint8_t *)attr_buf;
    size_t total = 0;

    while (1) {
        const smkfs_attr_header_t *ah = (const smkfs_attr_header_t *)ptr;
        total += sizeof(smkfs_attr_header_t) + ah->length;
        if (ah->type == SMKFS_ATTRT_END) break;
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }
    return total;
}