/*
    * fs/smkfs/bcache.c - [Enter description]
    * Author:   amity
    * Date:     Tue Aug 11 14:30:40 2026
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

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

/*
 * block_cache_init
 * Invalidate all cache slots on mount.
*/
VOID block_cache_init(_SMKFS_MOUNT *mnt) {
    ULONG i;

    if (!mnt->block_cache) {
        mnt->block_cache = (_SMKFS_BLOCK_BUF *)malloc(
            sizeof(_SMKFS_BLOCK_BUF) * SMKFS_CACHE_SLOTS);
        if (!mnt->block_cache) return;
    }

    for (i = 0; i < SMKFS_CACHE_SLOTS; i++) {
        mnt->block_cache[i].valid = 0;
        mnt->block_cache[i].dirty = 0;
        mnt->block_cache[i].block = 0;
    }
}

VOID block_cache_shutdown(_SMKFS_MOUNT *mnt) {
    /*
     * Never shut down with a non-empty cache
    */
    block_cache_flush(mnt);

    if (mnt && mnt->block_cache) {
        free(mnt->block_cache);
        mnt->block_cache = NULL;
    }
}

/*
 * block_cache_read
 * Look up block in per-mount cache. On miss, evict a slot and read
 * from disk. Returns a pointer to the cached buffer.
*/
SMKFS_STATUS block_cache_read(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block,  _SMKFS_BLOCK_BUF **out_buf) {
    ULONG i;
    _SMKFS_BLOCK_BUF *slot = NULL;

    if (!mnt || !out_buf) {
        return SMKFS_ERR_INVAL;
    }

    for (i = 0; i < SMKFS_CACHE_SLOTS; i++) {
        if (mnt->block_cache[i].valid &&
            mnt->block_cache[i].block == block) {
            *out_buf = &mnt->block_cache[i];
            return SMKFS_OK;
        }
    }

    for (i = 0; i < SMKFS_CACHE_SLOTS; i++) {
        if (!mnt->block_cache[i].valid) {
            slot = &mnt->block_cache[i];
            break;
        }
    }

    if (!slot) {
        for (i = 0; i < SMKFS_CACHE_SLOTS; i++) {
            if (!mnt->block_cache[i].dirty) {
                slot = &mnt->block_cache[i];
                break;
            }
        }
    }

    if (!slot) {
        slot = &mnt->block_cache[0];
        if (disk_write_block(mnt, slot->block, slot->data) != SMKFS_OK) {
            return SMKFS_ERR_IO;
        }
        slot->dirty = 0;
    }

    if (disk_read_block(mnt, block, slot->data) != SMKFS_OK) {
        slot->valid = 0;
        return SMKFS_ERR_IO;
    }

    slot->block = block;
    slot->valid = 1;
    slot->dirty = 0;
    *out_buf = slot;
    return SMKFS_OK;
}

/*
 * block_cache_write
 * Mark a cached buffer dirty. Caller must have obtained buf via
 * block_cache_read.
*/
SMKFS_STATUS block_cache_write(_SMKFS_MOUNT *mnt, _SMKFS_BLOCK_BUF *buf) {
    (VOID)mnt;

    if (!buf || !buf->valid) {
        return SMKFS_ERR_INVAL;
    }

    buf->dirty = 1;
    return SMKFS_OK;
}

/*
 * block_cache_flush
 * Write all dirty buffers back to disk.
*/
SMKFS_STATUS block_cache_flush(_SMKFS_MOUNT *mnt) {
    ULONG i;
    SMKFS_STATUS status = SMKFS_OK;

    if (!mnt) {
        return SMKFS_ERR_INVAL;
    }

    for (i = 0; i < SMKFS_CACHE_SLOTS; i++) {
        if (mnt->block_cache[i].valid && mnt->block_cache[i].dirty) {
            if (disk_write_block(mnt, mnt->block_cache[i].block, mnt->block_cache[i].data) != SMKFS_OK) {
                status = SMKFS_ERR_IO;
            } else {
                mnt->block_cache[i].dirty = 0;
            }
        }
    }

    return status;
}