/*
	* fs/smkfs/extent.c - Extent Management
	* Author:   amity
	* Date:     Wed Jul 29 17:38:38 2026
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

int extent_resolve(smkfs_mount_t *mnt, uint64_t record_id, uint64_t logical_block, smkfs_extent_t *out) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    int attr_len = record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf));
    if (attr_len < 0) return attr_len;

    void *attr_data;
    size_t attr_data_len = 0;
    if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &attr_data, &attr_data_len) != 0) {
        return SMKFS_ERR_NOTFOUND;
    }

    uint32_t num_extents = attr_data_len / sizeof(smkfs_extent_t);
    smkfs_extent_t *extents = (smkfs_extent_t *)attr_data;

    for (uint32_t i = 0; i < num_extents; i++) {
        if (logical_block >= extents[i].logical_offset && logical_block < extents[i].logical_offset + extents[i].block_count) {
            if (out) *out = extents[i];
            return SMKFS_OK;
        }
    }
    return SMKFS_ERR_NOTFOUND;
}

/*
 * TODO: optimize for G1 multi-valued extent attributes
 */
int extent_add(smkfs_mount_t *mnt, uint64_t record_id, uint64_t logical_block, uint64_t physical_block, uint32_t count) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_record_t *rec = (smkfs_record_t *)block;

    if (read_block(mnt, record_id, block) != 0) return SMKFS_ERR_IO;

    uint8_t *attr_buf = block + sizeof(smkfs_record_t);
    size_t attr_space = SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t);

    void *existing;
    size_t existing_len = 0;
    smkfs_extent_t extents[32];
    uint32_t num_extents = 0;

    if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &existing, &existing_len) == 0) {
        num_extents = existing_len / sizeof(smkfs_extent_t);
        if (num_extents >= 32) return SMKFS_ERR_TOO_BIG;
        memcpy(extents, existing, existing_len);
    }

    extents[num_extents].logical_offset = logical_block;
    extents[num_extents].physical_block = physical_block;
    extents[num_extents].block_count = count;
    num_extents++;

    record_remove_attr(attr_buf, SMKFS_ATTRT_EXTENTS);
    if (record_add_attr(attr_buf, attr_space, SMKFS_ATTRT_EXTENTS, extents, num_extents * sizeof(smkfs_extent_t)) != 0) {
        return SMKFS_ERR_NOSPC;
    }

    rec->attr_count = 0;
    rec->header.length = sizeof(smkfs_record_t);
    uint8_t *ptr = attr_buf;
    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        rec->header.length += sizeof(smkfs_attr_header_t) + ah->length;
        rec->attr_count++;
        if (ah->type == SMKFS_ATTRT_END) break;
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }
    rec->attr_count--;

    header_checksum_update(&rec->header, block, rec->header.length);
    return write_block(mnt, record_id, block);
}

void extent_remove_all(smkfs_mount_t *mnt, uint64_t record_id) {
    uint8_t *block = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!block) return;

    smkfs_record_t *rec = (smkfs_record_t *)block;
    if (read_block(mnt, record_id, block) != 0) {
        free(block);
        return;
    }

    uint8_t *attr_buf = block + sizeof(smkfs_record_t);
    void *existing;
    size_t existing_len = 0;
    if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &existing, &existing_len) != 0) {
        free(block);
        return;
    }

    uint32_t num_extents = existing_len / sizeof(smkfs_extent_t);
    smkfs_extent_t *extents = (smkfs_extent_t *)existing;
    for (uint32_t i = 0; i < num_extents; i++) {
        bitmap_free_range(mnt, extents[i].physical_block, extents[i].block_count);
    }

    record_remove_attr(attr_buf, SMKFS_ATTRT_EXTENTS);

    rec->header.length = sizeof(smkfs_record_t);
    uint8_t *ptr = attr_buf;
    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        rec->header.length += sizeof(smkfs_attr_header_t) + ah->length;
        if (ah->type == SMKFS_ATTRT_END) break;
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }

    header_checksum_update(&rec->header, block, rec->header.length);
    write_block(mnt, record_id, block);
    free(block);
}