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
static LONG extent_resolve_cb(SMKFS_ATTR_ID attr_id, PVOID data, SIZE_T len, PVOID ctx) {
    (VOID)attr_id;
    _SMKFS_ATTR_CTX *c = (_SMKFS_ATTR_CTX *)ctx;

    if (len != sizeof(_SMKFS_EXTENT)) return 0;

    _SMKFS_EXTENT *ext = (_SMKFS_EXTENT *)data;
    if (c->block >= ext->logical_offset && c->block < ext->logical_offset + ext->block_count) {
        if (c->out) *c->out = *ext;
        c->found = 1;
        return 1;
    }
    return 0;
}

SMKFS_STATUS extent_resolve(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, SMKFS_LBLOCK logical_block, _SMKFS_EXTENT *out) {
    UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
    _SMKFS_RECORD rec;
    SMKFS_STATUS status;
    _SMKFS_ATTR_CTX ctx;

    status = record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf));
    if (status < 0) return status;

    ctx.block = logical_block;
    ctx.out = out;
    ctx.found = 0;

    record_iterate_attr(attr_buf, SMKFS_ATTRT_EXTENTS, extent_resolve_cb, &ctx);
    
    return ctx.found ? SMKFS_OK : SMKFS_ERR_NOTFOUND;
}

static LONG extent_merge_cb(SMKFS_ATTR_ID attr_id, PVOID data,
                            SIZE_T len, PVOID ctx) {
    _SMKFS_EXT_MERGE_CTX *c = (_SMKFS_EXT_MERGE_CTX *)ctx;

    if (len != sizeof(_SMKFS_EXTENT)) return 0;

    _SMKFS_EXTENT *ext = (_SMKFS_EXTENT *)data;

    /* New follows ext */
    if (ext->logical_offset + ext->block_count == c->logical_block &&
        ext->physical_block + ext->block_count == c->physical_block) {
        c->merged.logical_offset = ext->logical_offset;
        c->merged.physical_block = ext->physical_block;
        c->merged.block_count = ext->block_count + c->count;
        c->matched_id = attr_id;
        c->found = 1;
        return 1;
    }

    /* New precedes ext */
    if (c->logical_block + c->count == ext->logical_offset &&
        c->physical_block + c->count == ext->physical_block) {
        c->merged.logical_offset = c->logical_block;
        c->merged.physical_block = c->physical_block;
        c->merged.block_count = c->count + ext->block_count;
        c->matched_id = attr_id;
        c->found = 1;
        return 1;
    }

    return 0;
}

SMKFS_STATUS extent_add(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, SMKFS_LBLOCK logical_block, SMKFS_BLOCK physical_block, ULONG count) {
    UCHAR block[SMKFS_BLOCK_SIZE];
    _SMKFS_RECORD *rec = (_SMKFS_RECORD *)block;

    SMKFS_BLOCK phys_block;
    SMKFS_STATUS mrt_ret = mrt_resolve(mnt, record_id, &phys_block, NULL, NULL);
    if (mrt_ret != SMKFS_OK) {
        return mrt_ret;
    }

    if (read_block(mnt, phys_block, block) != SMKFS_OK) return SMKFS_ERR_IO;

    PUCHAR attr_buf = block + sizeof(_SMKFS_RECORD);
    SIZE_T attr_space = SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD);

    _SMKFS_EXT_MERGE_CTX ctx;
    ctx.logical_block = logical_block;
    ctx.physical_block = physical_block;
    ctx.count = count;
    ctx.found = 0;

    record_iterate_attr(attr_buf, SMKFS_ATTRT_EXTENTS, extent_merge_cb, &ctx);

    if (ctx.found) {
        record_remove_attr_id(attr_buf, SMKFS_ATTRT_EXTENTS, ctx.matched_id);
        if (record_add_attr(attr_buf, attr_space, SMKFS_ATTRT_EXTENTS,
                            &ctx.merged, sizeof(ctx.merged)) != SMKFS_OK) {
            return SMKFS_ERR_NOSPC;
        }
    } else {
        _SMKFS_EXTENT new_ext;
        new_ext.logical_offset = logical_block;
        new_ext.physical_block = physical_block;
        new_ext.block_count = count;
        if (record_add_attr(attr_buf, attr_space, SMKFS_ATTRT_EXTENTS,
                            &new_ext, sizeof(new_ext)) != SMKFS_OK) {
            return SMKFS_ERR_NOSPC;
        }
    }

    rec->header.length = sizeof(_SMKFS_RECORD);
    PUCHAR ptr = attr_buf;
    while (1) {
        _SMKFS_ATTR_HEADER *ah = (_SMKFS_ATTR_HEADER *)ptr;
        rec->header.length += sizeof(_SMKFS_ATTR_HEADER) + ah->length;
        rec->attr_count++;
        if (ah->type == SMKFS_ATTRT_END) break;
        ptr += sizeof(_SMKFS_ATTR_HEADER) + ah->length;
    }
    rec->attr_count--;

    header_checksum_update(&rec->header, block, rec->header.length);
    return write_block(mnt, phys_block, block);
}

static LONG extent_remove_cb(SMKFS_ATTR_ID attr_id, PVOID data, SIZE_T len, PVOID ctx) {
    (VOID)attr_id;
    _SMKFS_EXT_REMOVE_CTX *c = (_SMKFS_EXT_REMOVE_CTX *)ctx;
    
    if (len != sizeof(_SMKFS_EXTENT)) {
        return 0;
    }

    if (c->count >= 32) {
        return 0;
    }

    _SMKFS_EXTENT *ext = (_SMKFS_EXTENT *)data;

    c->extents[c->count] = *ext;
    c->count++;

    return 0;
}

void extent_remove_all(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id) {
    PUCHAR block = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!block) return;

    _SMKFS_RECORD *rec = (_SMKFS_RECORD *)block;

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

    PUCHAR attr_buf = block + sizeof(_SMKFS_RECORD);

    _SMKFS_EXTENT extents[32];
    _SMKFS_EXT_REMOVE_CTX ctx;

    ctx.extents = extents;
    ctx.count = 0;

    record_iterate_attr(attr_buf, SMKFS_ATTRT_EXTENTS, extent_remove_cb, &ctx);

    for (ULONG i = 0; i < ctx.count; i++) {
        bitmap_free_range(mnt, extents[i].physical_block, extents[i].block_count);
    }

    record_remove_attr(attr_buf, SMKFS_ATTRT_EXTENTS);

    rec->header.length = sizeof(_SMKFS_RECORD);
    PUCHAR ptr = attr_buf;
    while (1) {
        _SMKFS_ATTR_HEADER *ah = (_SMKFS_ATTR_HEADER *)ptr;
        rec->header.length += sizeof(_SMKFS_ATTR_HEADER) + ah->length;
        if (ah->type == SMKFS_ATTRT_END) break;
        ptr += sizeof(_SMKFS_ATTR_HEADER) + ah->length;
    }

    header_checksum_update(&rec->header, block, rec->header.length);
    write_block(mnt, phys_block, block);
    free(block);
}