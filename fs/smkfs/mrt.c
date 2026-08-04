/*
	* fs/smkfs/mrt.c - Master Record Table (G1)
	* Author:   amity
	* Date:     Thu Jul 30 13:18:15 2026
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
#include <internal/phonon_macros.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/
inline int power_of_two(int n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

int mrt_format(smkfs_mount_t *mnt, uint64_t start_block, uint64_t length) {
    
    /* 0 length MRT is invalid*/
    ASSERT(length != 0);
    
    /* MRT entry size must be power of two for capacity to be calculated nicely */
    /* SMKFS_BLOCK_SIZE = 4096 = 2^12, so only powers of two are divisors*/
    ASSERT(power_of_two(sizeof(smkfs_mrt_entry_t)) != 0);

    uint8_t block[SMKFS_BLOCK_SIZE];
    uint32_t entries_per_block = SMKFS_BLOCK_SIZE / sizeof(smkfs_mrt_entry_t);
    smkfs_mrt_entry_t *entries = (smkfs_mrt_entry_t *)block;

    /* Write j entries to the i'th block */
    for (uint64_t i = 0; i < length; i++) {
        for (uint32_t j = 0; j < entries_per_block; j++) {
            entries[j].physical_block = UINT64_MAX;
            entries[j].flags = 0;
            entries[j].reserved = 0;
            entries[j].generation = 0;
        }


        if (write_block(mnt,start_block + i, block)) {
            return SMKFS_ERR_IO;
        }
    }

    return SMKFS_OK;
}

int mrt_init(smkfs_mount_t *mnt, uint64_t start_block, uint64_t length) {

    /* 0 length MRT is invalid*/
    ASSERT(length != 0);
    
    /* MRT entry size must be power of two for capacity to be calculated nicely */
    /* SMKFS_BLOCK_SIZE = 4096 = 2^12, so only powers of two are divisors*/
    ASSERT(power_of_two(sizeof(smkfs_mrt_entry_t)) != 0);

    mnt->sb.mrt_start = start_block;
    mnt->sb.mrt_length = length;
    mnt->sb.mrt_capacity = length * (SMKFS_BLOCK_SIZE / sizeof(smkfs_mrt_entry_t));
    return SMKFS_OK;
}

int mrt_alloc_entry(smkfs_mount_t *mnt, uint64_t *out_record_id, uint64_t *out_generation) {

    uint8_t block[SMKFS_BLOCK_SIZE];

    /* No check necessary, if it would fail, the mrt couldn't be initialized
     * or kernel code is being modified while running.
     * This would mean a bug or attack, best to check anyway.
    */
    ASSERT(power_of_two(sizeof(smkfs_mrt_entry_t)) != 0);

    uint32_t entries_per_block = SMKFS_BLOCK_SIZE / sizeof(smkfs_mrt_entry_t);

    for (uint64_t i = 0; i < mnt->sb.mrt_length; i++) {
        if (read_block(mnt, mnt->sb.mrt_start + i, block) != 0) {
            return SMKFS_ERR_IO;
        }

        smkfs_mrt_entry_t *entries = (smkfs_mrt_entry_t *)block;
        for (uint32_t j = 0; j < entries_per_block; j++) {

            /* MEOW flashback */
            uint64_t candidate = i * entries_per_block + j;
            if (candidate == 0) continue; /* reserved for Superblock */
            if (entries[j].flags & SMKFS_MRTF_ALLOCATED) continue; /* Already in use */

            entries[j].flags |= SMKFS_MRTF_ALLOCATED;
            entries[j].physical_block = UINT64_MAX;
            entries[j].generation++;

            if (write_block(mnt, mnt->sb.mrt_start + i, block) != 0) {
                return SMKFS_ERR_IO;
            }

            mnt->sb.mrt_free_count--;
            *out_record_id = candidate;
            *out_generation = entries[j].generation;
            return SMKFS_OK;
        }
    }

    return SMKFS_ERR_NOSPC;
}

int mrt_update_entry(smkfs_mount_t *mnt, uint64_t record_id, uint64_t new_physical_block, uint16_t flags) {
    uint8_t block[SMKFS_BLOCK_SIZE];

    /* I'm not gonna reason this one again */
    ASSERT(power_of_two(sizeof(smkfs_mrt_entry_t)) != 0);

    if (record_id >= mnt->sb.mrt_capacity) {
        return SMKFS_ERR_INVAL;
    }

    uint32_t entries_per_block = SMKFS_BLOCK_SIZE / sizeof(smkfs_mrt_entry_t);

    uint64_t block_id = mnt->sb.mrt_start + record_id / entries_per_block;
    if (read_block(mnt, block_id, block) != 0) {
        return SMKFS_ERR_IO;
    }

    smkfs_mrt_entry_t *entries = (smkfs_mrt_entry_t *)block;
    uint32_t entry_id = record_id % entries_per_block;

    entries[entry_id].physical_block = new_physical_block;
    entries[entry_id].flags |= flags;

    if (write_block(mnt, block_id, block) != 0) {
        return SMKFS_ERR_IO;
    }
    
    return SMKFS_OK;
}

int mrt_free_entry(smkfs_mount_t *mnt, uint64_t record_id) {

    uint8_t block[SMKFS_BLOCK_SIZE];

    /* I'm not gonna reason this one again */
    ASSERT(power_of_two(sizeof(smkfs_mrt_entry_t)) != 0);

    if (record_id >= mnt->sb.mrt_capacity) {
        return SMKFS_ERR_INVAL;
    }

    uint32_t entries_per_block = SMKFS_BLOCK_SIZE / sizeof(smkfs_mrt_entry_t);

    uint64_t block_id = mnt->sb.mrt_start + record_id / entries_per_block;
    if (read_block(mnt, block_id, block) != 0) {
        return SMKFS_ERR_IO;
    }

    smkfs_mrt_entry_t *entries = (smkfs_mrt_entry_t *)block;
    uint32_t entry_id = record_id % entries_per_block;

    entries[entry_id].physical_block = UINT64_MAX; /* 64 ZiB, safe for now */
    entries[entry_id].flags = 0; /* Zero flags, also means unallocated */
    /*
     * alloc_entry increases generation when record is allocated, no need to do it here.
     * entry has to be allocated again for reuse.
    */

    if (write_block(mnt, block_id, block) != 0) {
        return SMKFS_ERR_IO;
    }

    mnt->sb.mrt_free_count++;

    return SMKFS_OK;
}

int mrt_resolve(smkfs_mount_t *mnt, uint64_t record_id, uint64_t *out_physical_block, uint16_t *out_flags, uint32_t *out_generation) {
    
    uint8_t block[SMKFS_BLOCK_SIZE];

    /* I'm not gonna reason this one again */
    ASSERT(power_of_two(sizeof(smkfs_mrt_entry_t)) != 0);

    if (record_id >= mnt->sb.mrt_capacity) {
        return SMKFS_ERR_INVAL;
    }

    uint32_t entries_per_block = SMKFS_BLOCK_SIZE / sizeof(smkfs_mrt_entry_t);

    uint64_t block_id = mnt->sb.mrt_start + record_id / entries_per_block;
    if (read_block(mnt, block_id, block) != 0) {
        return SMKFS_ERR_IO;
    }

    smkfs_mrt_entry_t *entries = (smkfs_mrt_entry_t *)block;
    uint32_t entry_id = record_id % entries_per_block;
    smkfs_mrt_entry_t entry = entries[entry_id];

    if (!(entry.flags & SMKFS_MRTF_ALLOCATED)) {
        return SMKFS_ERR_NOTFOUND;
    }

    if (entry.physical_block == 0 || entry.physical_block == UINT64_MAX) {
        return SMKFS_ERR_NOT_YET_BOUND;
    }

    if (out_physical_block) *out_physical_block = entry.physical_block;
    if (out_flags) *out_flags = entry.flags;
    if (out_generation) *out_generation = entry.generation;

    return SMKFS_OK;
}