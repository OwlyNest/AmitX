/*
	* fs/smkfs/bitmap.c - Bitmap Allocation
	* Author:   amity
	* Date:     Wed Jul 29 17:38:13 2026
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
#include <screen/printk.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

void bitmap_set(smkfs_mount_t *mnt, uint64_t block) {
    uint8_t *buf;

    if (block < mnt->sb.data_start) return;
    uint64_t idx = block - mnt->sb.data_start;
    uint64_t byte_offset = idx / 8;
    uint64_t bit_offset = idx % 8;
    uint64_t bitmap_block = mnt->sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
    uint64_t block_offset = byte_offset % SMKFS_BLOCK_SIZE;

    buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return;
    if (read_block(mnt, bitmap_block, buf) != 0) {
        free(buf);
        return;
    }

    if (((buf[block_offset] >> bit_offset) & 1) == 0) {
        mnt->sb.free_blocks--;
    }

    buf[block_offset] |= (1 << bit_offset);
    if (write_block(mnt, bitmap_block, buf) != 0) {
        free(buf);
        return;
    }

    free(buf);
}

void bitmap_clear(smkfs_mount_t *mnt, uint64_t block) {
    uint8_t *buf;

    if (block < mnt->sb.data_start) return;
    uint64_t idx = block - mnt->sb.data_start;
    uint64_t byte_offset = idx / 8;
    uint64_t bit_offset = idx % 8;
    uint64_t bitmap_block = mnt->sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
    uint64_t block_offset = byte_offset % SMKFS_BLOCK_SIZE;

    buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return;
    if (read_block(mnt, bitmap_block, buf) != 0) {
        free(buf);
        return;
    }

    if (((buf[block_offset] >> bit_offset) & 1) != 0) {
        mnt->sb.free_blocks++;
    }
    buf[block_offset] &= ~(1 << bit_offset);
    if (write_block(mnt, bitmap_block, buf) != 0) {
        free(buf);
        return;
    }

    free(buf);
}

int bitmap_test(smkfs_mount_t *mnt, uint64_t block) {
    uint8_t *buf;
    int ret;

    if (block < mnt->sb.data_start) return -1;
    uint64_t idx = block - mnt->sb.data_start;
    uint64_t byte_offset = idx / 8;
    uint64_t bit_offset = idx % 8;
    uint64_t bitmap_block = mnt->sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
    uint64_t block_offset = byte_offset % SMKFS_BLOCK_SIZE;

    buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return -1;
    if (read_block(mnt, bitmap_block, buf) != 0) {
        free(buf);
        return -1;
    }

    ret = (buf[block_offset] >> bit_offset) & 1;
    free(buf);
    return ret;
}

/*
 * TODO: optimize for G1 regioned allocator
 */
uint64_t bitmap_alloc_range(smkfs_mount_t *mnt, uint32_t count) {
    uint8_t *buf;
    uint8_t *j_buf;
    uint64_t total_bits;
    uint64_t run_start;
    uint32_t run_len;

    if (count == 0) return 0;

    buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    j_buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!buf || !j_buf) {
        free(buf);
        free(j_buf);
        return 0;
    }

    total_bits = mnt->sb.total_blocks - mnt->sb.data_start;
    run_start = 0;
    run_len = 0;

    for (uint64_t bb = 0; bb < mnt->sb.bitmap_length; bb++) {
        if (read_block(mnt, mnt->sb.bitmap_start + bb, buf) != 0) {
            free(buf);
            free(j_buf);
            return 0;
        }

        for (int bo = 0; bo < SMKFS_BLOCK_SIZE; bo++) {
            uint8_t byte = buf[bo];
            for (int bi = 0; bi < 8; bi++) {
                uint64_t global_bit = bb * SMKFS_BLOCK_SIZE * 8 + bo * 8 + bi;
                if (global_bit >= total_bits) break;

                if ((byte >> bi) & 1) {
                    run_len = 0;
                } else {
                    if (run_len == 0) run_start = global_bit;
                    run_len++;
                    if (run_len >= count) {
                        uint64_t first_block = mnt->sb.data_start + run_start;
                        for (uint32_t j = 0; j < count; j++) {
                            uint64_t idx = run_start + j;
                            uint64_t j_bb = idx / (SMKFS_BLOCK_SIZE * 8);
                            uint64_t j_bo = (idx % (SMKFS_BLOCK_SIZE * 8)) / 8;
                            uint64_t j_bi = idx % 8;
                            if (read_block(mnt, mnt->sb.bitmap_start + j_bb, j_buf) != 0) {
                                free(buf);
                                free(j_buf);
                                return 0;
                            }

                            j_buf[j_bo] |= (1 << j_bi);
                            if (write_block(mnt, mnt->sb.bitmap_start + j_bb, j_buf) != 0) {
                                free(buf);
                                free(j_buf);
                                return 0;
                            }
                        }

                        mnt->sb.free_blocks -= count;
                        journal_log_alloc(mnt, first_block, count);
                        free(buf);
                        free(j_buf);
                        return first_block;
                    }
                }
            }
        }
    }

    free(buf);
    free(j_buf);
    return 0;
}

uint64_t bitmap_alloc(smkfs_mount_t *mnt) {
    return bitmap_alloc_range(mnt, 1);
}

void bitmap_free_range(smkfs_mount_t *mnt, uint64_t start, uint32_t count) {
    uint8_t *buf;

    buf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return;

    for (uint32_t i = 0; i < count; i++) {
        uint64_t block = start + i;
        if (block < mnt->sb.data_start) continue;
        uint64_t idx = block - mnt->sb.data_start;
        uint64_t bb = idx / (SMKFS_BLOCK_SIZE * 8);
        uint64_t bo = (idx % (SMKFS_BLOCK_SIZE * 8)) / 8;
        uint64_t bi = idx % 8;

        if (read_block(mnt, mnt->sb.bitmap_start + bb, buf) != 0) {
            free(buf);
            return;
        }

        buf[bo] &= ~(1 << bi);
        if (write_block(mnt, mnt->sb.bitmap_start + bb, buf) != 0) {
            free(buf);
            return;
        }
    }

    journal_log_free(mnt, start, count);
    mnt->sb.free_blocks += count;
    free(buf);
}