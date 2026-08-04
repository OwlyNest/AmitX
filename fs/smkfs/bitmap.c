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

VOID bitmap_set(smkfs_mount_t *mnt, ULONGLONG block) {
    PUCHAR buf;

    if (block < mnt->sb.data_start) return;
    ULONGLONG idx = block - mnt->sb.data_start;
    ULONGLONG byte_offset = idx / 8;
    ULONGLONG bit_offset = idx % 8;
    ULONGLONG bitmap_block = mnt->sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
    ULONGLONG block_offset = byte_offset % SMKFS_BLOCK_SIZE;

    buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
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

VOID bitmap_clear(smkfs_mount_t *mnt, ULONGLONG block) {
    PUCHAR buf;

    if (block < mnt->sb.data_start) return;
    ULONGLONG idx = block - mnt->sb.data_start;
    ULONGLONG byte_offset = idx / 8;
    ULONGLONG bit_offset = idx % 8;
    ULONGLONG bitmap_block = mnt->sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
    ULONGLONG block_offset = byte_offset % SMKFS_BLOCK_SIZE;

    buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
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

SMKFS_STATUS bitmap_test(smkfs_mount_t *mnt, ULONGLONG block) {
    PUCHAR buf;
    int ret;

    if (block < mnt->sb.data_start) return -1;
    ULONGLONG idx = block - mnt->sb.data_start;
    ULONGLONG byte_offset = idx / 8;
    ULONGLONG bit_offset = idx % 8;
    ULONGLONG bitmap_block = mnt->sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
    ULONGLONG block_offset = byte_offset % SMKFS_BLOCK_SIZE;

    buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
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
ULONGLONG bitmap_alloc_range(smkfs_mount_t *mnt, ULONG count) {
    PUCHAR buf;
    PUCHAR j_buf;
    ULONGLONG total_bits;
    ULONGLONG run_start;
    ULONG run_len;

    if (count == 0) return 0;

    buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    j_buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!buf || !j_buf) {
        free(buf);
        free(j_buf);
        return 0;
    }

    total_bits = mnt->sb.total_blocks - mnt->sb.data_start;
    run_start = 0;
    run_len = 0;

    for (ULONGLONG bb = 0; bb < mnt->sb.bitmap_length; bb++) {
        if (read_block(mnt, mnt->sb.bitmap_start + bb, buf) != 0) {
            free(buf);
            free(j_buf);
            return 0;
        }

        for (LONG bo = 0; bo < SMKFS_BLOCK_SIZE; bo++) {
            BYTE byte = buf[bo];
            for (int bi = 0; bi < 8; bi++) {
                ULONGLONG global_bit = bb * SMKFS_BLOCK_SIZE * 8 + bo * 8 + bi;
                if (global_bit >= total_bits) break;

                if ((byte >> bi) & 1) {
                    run_len = 0;
                } else {
                    if (run_len == 0) run_start = global_bit;
                    run_len++;
                    if (run_len >= count) {
                        ULONGLONG first_block = mnt->sb.data_start + run_start;
                        for (uint32_t j = 0; j < count; j++) {
                            ULONGLONG idx = run_start + j;
                            ULONGLONG j_bb = idx / (SMKFS_BLOCK_SIZE * 8);
                            ULONGLONG j_bo = (idx % (SMKFS_BLOCK_SIZE * 8)) / 8;
                            ULONGLONG j_bi = idx % 8;
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

ULONGLONG bitmap_alloc(smkfs_mount_t *mnt) {
    return bitmap_alloc_range(mnt, 1);
}

VOID bitmap_free_range(smkfs_mount_t *mnt, ULONGLONG start, ULONG count) {
    PUCHAR buf;

    buf = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!buf) return;

    for (ULONG i = 0; i < count; i++) {
        ULONGLONG block = start + i;
        if (block < mnt->sb.data_start) continue;
        ULONGLONG idx = block - mnt->sb.data_start;
        ULONGLONG bb = idx / (SMKFS_BLOCK_SIZE * 8);
        ULONGLONG bo = (idx % (SMKFS_BLOCK_SIZE * 8)) / 8;
        ULONGLONG bi = idx % 8;

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