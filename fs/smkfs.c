/*
	* fs/smkfs.c - [Enter description]
	* Author:   amity
	* Date:     Thu Jul 23 11:40:03 2026
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
#include <hw/ide.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <stdint.h>
#include <stddef.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static smkfs_superblock_t sb;
static int mounted = 0;
static uint8_t drive_num = 0;
static uint64_t journal_next_sequence = 1;
static int journal_in_transaction = 0;
/* --- Prototypes ---*/

/* ~~~ Level 4: Internal ~~~ */
/* Block I/O */
static int read_block(uint64_t block, void *buf);
static int write_block(uint64_t block, const void *buf);

/* Header */
static void header_init(smkfs_header_t *h, uint16_t type, uint32_t length, uint32_t flags);
static int header_validate(const smkfs_header_t *h, uint16_t expected_type);

/* Checksum */
static uint32_t checksum_compute(const void *data, size_t len);
static void header_checksum_update(smkfs_header_t *h, const void *data, size_t len);
static int header_checksum_verify(const smkfs_header_t *h, const void *data, size_t len);

/* Bitmap */
static void bitmap_set(uint64_t block);
static void bitmap_clear(uint64_t block);
static int bitmap_test(uint64_t block);
static uint64_t bitmap_alloc_range(uint32_t count);
static uint64_t bitmap_alloc(void);
static void bitmap_free_range(uint64_t start, uint32_t count);

/* Record */
static int record_read(uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size);
static int record_write(uint64_t record_id, const smkfs_record_t *rec, const void *attr_buf);
static uint64_t record_alloc(uint16_t object_type);
static void record_free(uint64_t record_id);
static int record_find_attr(const void *attr_buf, uint16_t attr_type, void **out_attr, size_t out_len);
static int record_add_attr(void *attr_buf, size_t buf_size, uint16_t attr_type, const void *data, size_t data_len);
static int record_remove_attr(void *attr_buf, uint16_t attr_type);

/* Extent */
static int extent_resolve(uint64_t record_id, uint64_t logical_block, smkfs_extent_t *out);
static int extent_add(uint64_t record_id, uint64_t logical_block, uint64_t physical_block, uint32_t out);
static void extent_remove_all(uint64_t record_id);

/* B+ tree */
static int btree_search(uint64_t root_block, uint64_t key_hash, uint64_t *out_value);
static int btree_insert(uint64_t root_block, uint64_t key_hash, uint64_t value, uint64_t *new_root);
static int btree_delete(uint64_t root_block, uint64_t key_hash, uint64_t *new_root);
static int btree_iterate(uint64_t root_block, int (*cb)(uint64_t key, uint64_t value, void *ctx), void *ctx);

/* Journal */
static int journal_start_transaction(void);
static int journal_log_write(uint64_t block, const void *old_data, const void *new_data, size_t len);
static int journal_log_alloc(uint64_t block, uint32_t count);
static int journal_log_free(uint64_t block, uint32_t count);
static int journal_commit(void);
/* --- Functions ---*/


/* ~~~ Level 4: Internal ~~~ */

/* Block I/O */

static int read_block(uint64_t block, void *buf) {
	uint8_t sectors = SMKFS_BLOCK_SIZE / SMKFS_SECTOR_SIZE;
	uint32_t lba = (uint32_t)(block * sectors);
	uint8_t *ptr = (uint8_t *)buf;


	for (uint8_t i = 0; i < sectors; i++) {
		if (ide_read_sectors(drive_num, lba + 1, 1, (uint16_t *)(ptr + i * SMKFS_SECTOR_SIZE)) != 0) {
			return -1;
		}
	}

	return 0;
}

static int write_block(uint64_t block, const void *buf) {
	uint8_t sectors = SMKFS_BLOCK_SIZE / SMKFS_SECTOR_SIZE;
	uint32_t lba = (uint32_t)(block * sectors);
	const uint8_t *ptr = (const uint8_t *)buf;

	for (uint8_t i = 0; i < sectors; i++) {
		if (ide_write_sectors(drive_num, lba + 1, 1, (const uint16_t *)(ptr + i *SMKFS_SECTOR_SIZE)) != 0) {
			return -1;
		}
	}

	return 0;
}

/* Header */

static void header_init(smkfs_header_t *h, uint16_t type, uint32_t length, uint32_t flags) {
	memcpy(h->magic, SMKFS_MAGIC, 4);
	h->version = SMKFS_VERSION;
	h->type = type;
	h->length = length;
	h->flags = flags;
	h->checksum = 0;
}

static int header_validate(const smkfs_header_t *h, uint16_t expected_type) {
	if (memcmp(h->magic, SMKFS_MAGIC, 4) != 0) {
		return -1;
	}

	if (h->version != SMKFS_VERSION) {
		return -1;
	}

	if (h->type != expected_type) {
		return -1;
	}

	return 0;
}

/* Checksum */
static uint32_t checksum_compute(const void *data, size_t len) {
	const uint8_t *ptr = (const uint8_t *)data;
	uint32_t sum = 0xFFFFFF;

	for (size_t i = 0; i < len; i++) {
		sum ^= ptr[i];
		for (int j = 0; j < 8; j++) {
			if (sum & 1) {
				sum = (sum >> 1) ^ 0xEDB88320;
			} else {
				sum >>= 1;
			}
		}
	}

	return ~sum;
}

static void header_checksum_update(smkfs_header_t *h, const void *data, size_t len) {
	h->checksum = 0; // compute assumes checksum == 0;
	h->checksum = checksum_compute(data, len);
}

static int header_checksum_verify(const smkfs_header_t *h, const void *data, size_t len) {
	uint32_t saved = h->checksum;

	smkfs_header_t *mutable_h = (smkfs_header_t *)h;
	mutable_h->checksum = 0;
	uint32_t computed = checksum_compute(data, len);
	mutable_h->checksum = computed;

	if (computed != saved) {
		return -1;
	}

	return 0;
}

/* Bitmap */
static void bitmap_set(uint64_t block) {
	if (block < sb.data_start) return;
	uint64_t idx = block - sb.data_start;
	uint64_t byte_offset = idx / 8;
	uint64_t bit_offset = idx % 8;
	uint64_t bitmap_block = sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
	uint64_t block_offset = byte_offset % SMKFS_BLOCK_SIZE;

	uint8_t buf[SMKFS_BLOCK_SIZE];
	read_block(bitmap_block, buf);
	buf[block_offset] |= (1 << bit_offset);
	write_block(bitmap_block, buf);
}

static void bitmap_clear(uint64_t block) {
	if (block < sb.data_start) return;
	uint64_t idx = block - sb.data_start;
	uint64_t byte_offset = idx / 8;
	uint64_t bit_offset = idx % 8;
	uint64_t bitmap_block = sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
	uint64_t block_offset = byte_offset % SMKFS_BLOCK_SIZE;

	uint8_t buf[SMKFS_BLOCK_SIZE];
	read_block(bitmap_block, buf);
	buf[block_offset] &= ~(1 << bit_offset);
	write_block(bitmap_block, buf);
}

static int bitmap_test(uint64_t block){
	if (block < sb.data_start) return -1;
	uint64_t idx = block - sb.data_start;
	uint64_t byte_offset = idx / 8;
	uint64_t bit_offset = idx % 8;
	uint64_t bitmap_block = sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
	uint64_t block_offset = byte_offset % SMKFS_BLOCK_SIZE;

	uint8_t buf[SMKFS_BLOCK_SIZE];
	read_block(bitmap_block, buf);
	return (buf[block_offset] >> bit_offset) & 1;
}

static uint64_t bitmap_alloc_range(uint32_t count) {
	if (count == 0) return -1;

	uint64_t total_bits = sb.total_blocks - sb.data_start;
	uint64_t run_start = 0;
	uint32_t run_len = 0;

	for (uint64_t bb = 0; bb < sb.bitmap_length; bb++) {
		uint8_t buf[SMKFS_BLOCK_SIZE];
		read_block(sb.bitmap_start + bb, buf);

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
						uint64_t first_block = sb.data_start + run_start;
						for (uint32_t j = 0; j < count; j++) {
							uint64_t idx = run_start + j;
                            uint64_t j_bb = idx / (SMKFS_BLOCK_SIZE * 8);
                            uint64_t j_bo = (idx % (SMKFS_BLOCK_SIZE * 8)) / 8;
                            uint64_t j_bi = idx % 8;
                            uint8_t j_buf[SMKFS_BLOCK_SIZE];
                            read_block(sb.bitmap_start + j_bb, j_buf);
                            j_buf[j_bo] |= (1 << j_bi);
                            write_block(sb.bitmap_start + j_bb, j_buf);
						}
						sb.free_blocks -= count;
						return first_block;
					}
				}
			}
		}
	}
	return -1;
}

static uint64_t bitmap_alloc(void) {
	return bitmap_alloc_range(1);
}

static void bitmap_free_range(uint64_t start, uint32_t count) {
	for (uint32_t i = 0; i < count; i++) {
		uint64_t block = start + i;
        if (block < sb.data_start) continue;
        uint64_t idx = block - sb.data_start;
        uint64_t bb = idx / (SMKFS_BLOCK_SIZE * 8);
        uint64_t bo = (idx % (SMKFS_BLOCK_SIZE * 8)) / 8;
        uint64_t bi = idx % 8;

        uint8_t buf[SMKFS_BLOCK_SIZE];
        read_block(sb.bitmap_start + bb, buf);
        buf[bo] &= ~(1 << bi);
        write_block(sb.bitmap_start + bb, buf);
	}
	sb.free_blocks += count;
}

