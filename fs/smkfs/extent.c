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

SMKFS_STATUS extent_resolve(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id, SMKFS_LBLOCK logical_block, smkfs_extent_t *out) {
    UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    SMKFS_STATUS status;

    status = record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf));
    if (status < 0) return status;

    PVOID attr_data;
    SIZE_T attr_data_len = 0;
    if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &attr_data, &attr_data_len) != SMKFS_OK) {
        return SMKFS_ERR_NOTFOUND;
    }

    ULONG num_extents = attr_data_len / sizeof(smkfs_extent_t);
    smkfs_extent_t *extents = (smkfs_extent_t *)attr_data;

    for (ULONG i = 0; i < num_extents; i++) {
        if (logical_block >= extents[i].logical_offset && logical_block < extents[i].logical_offset + extents[i].block_count) {
            if (out) *out = extents[i];
            return SMKFS_OK;
        }
    }
    return SMKFS_ERR_NOTFOUND;
}

SMKFS_STATUS extent_add(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id, SMKFS_LBLOCK logical_block, SMKFS_BLOCK physical_block, ULONG count) {
    UCHAR block[SMKFS_BLOCK_SIZE];
    smkfs_record_t *rec = (smkfs_record_t *)block;

    SMKFS_BLOCK phys_block;
    SMKFS_STATUS mrt_ret = mrt_resolve(mnt, record_id, &phys_block, NULL, NULL);
    if (mrt_ret != SMKFS_OK) {
        return mrt_ret;
    }

    if (read_block(mnt, phys_block, block) != SMKFS_OK) return SMKFS_ERR_IO;

    PUCHAR attr_buf = block + sizeof(smkfs_record_t);
    SIZE_T attr_space = SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t);

    PVOID existing;
    SIZE_T existing_len = 0;
    smkfs_extent_t extents[32];
    ULONG num_extents = 0;

    if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &existing, &existing_len) == SMKFS_OK) {
        num_extents = existing_len / sizeof(smkfs_extent_t);
        if (num_extents >= 32) return SMKFS_ERR_TOO_BIG;
        memcpy(extents, existing, existing_len);
    }

    extents[num_extents].logical_offset = logical_block;
    extents[num_extents].physical_block = physical_block;
    extents[num_extents].block_count = count;
    num_extents++;

    record_remove_attr(attr_buf, SMKFS_ATTRT_EXTENTS);
    if (record_add_attr(attr_buf, attr_space, SMKFS_ATTRT_EXTENTS, extents, num_extents * sizeof(smkfs_extent_t)) != SMKFS_OK) {
        return SMKFS_ERR_NOSPC;
    }

    rec->attr_count = 0;
    rec->header.length = sizeof(smkfs_record_t);
    PUCHAR ptr = attr_buf;
    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        rec->header.length += sizeof(smkfs_attr_header_t) + ah->length;
        rec->attr_count++;
        if (ah->type == SMKFS_ATTRT_END) break;
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }
    rec->attr_count--;

    header_checksum_update(&rec->header, block, rec->header.length);
    return write_block(mnt, phys_block, block);
}

void extent_remove_all(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id) {
    PUCHAR block = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!block) return;

    smkfs_record_t *rec = (smkfs_record_t *)block;

    SMKFS_BLOCK phys_block;
    SMKFS_STATUS mrt_ret = mrt_resolve(mnt, record_id, &phys_block, NULL, NULL);
    if (mrt_ret != SMKFS_OK) {
        free(block);
        return;
    }

    if (read_block(mnt, phys_block, block) != SMKFS_OK) {
        free(block);
        return;
    }

    PUCHAR attr_buf = block + sizeof(smkfs_record_t);
    PVOID existing;
    SIZE_T existing_len = 0;
    if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &existing, &existing_len) != SMKFS_OK) {
        free(block);
        return;
    }

    ULONG num_extents = existing_len / sizeof(smkfs_extent_t);
    smkfs_extent_t *extents = (smkfs_extent_t *)existing;
    for (ULONG i = 0; i < num_extents; i++) {
        bitmap_free_range(mnt, extents[i].physical_block, extents[i].block_count);
    }

    record_remove_attr(attr_buf, SMKFS_ATTRT_EXTENTS);

    rec->header.length = sizeof(smkfs_record_t);
    PUCHAR ptr = attr_buf;
    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        rec->header.length += sizeof(smkfs_attr_header_t) + ah->length;
        if (ah->type == SMKFS_ATTRT_END) break;
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }

    header_checksum_update(&rec->header, block, rec->header.length);
    write_block(mnt, phys_block, block);
    free(block);
}