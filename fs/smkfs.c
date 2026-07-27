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
#define SMKFS_UNUSED __attribute__((unused))

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
static uint64_t journal_write_pos = 0;
static smkfs_fd_t fd_table[SMKFS_FD_MAX];
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
static int record_find_attr(const void *attr_buf, uint16_t attr_type, void **out_attr, size_t *out_len);
static int record_add_attr(void *attr_buf, size_t buf_size, uint16_t attr_type, const void *data, size_t data_len);
static int record_remove_attr(void *attr_buf, uint16_t attr_type);

/* Extent */
static int extent_resolve(uint64_t record_id, uint64_t logical_block, smkfs_extent_t *out);
static int extent_add(uint64_t record_id, uint64_t logical_block, uint64_t physical_block, uint32_t out);
static void extent_remove_all(uint64_t record_id);

/* B+ tree */
static int btree_search(uint64_t root_block, const char *key, uint64_t *out_value);
static int btree_insert(uint64_t root_block, const char *key, uint64_t value, uint64_t *new_root);
static int btree_leaf_insert_entry(smkfs_btree_node_t *node, smkfs_btree_leaf_entry_t *entries, uint32_t *count, const char *key, uint64_t value);
static int btree_delete(uint64_t root_block, const char *key, uint64_t *new_root);
static int btree_iterate(uint64_t root_block, int (*cb)(const char *key, uint64_t value, void *ctx), void *ctx);

/* Journal */
static int journal_start_transaction(void);
static int journal_log_write(uint64_t block, const void *old_data, const void *new_data, size_t len);
static int journal_log_alloc(uint64_t block, uint32_t count);
static int journal_log_free(uint64_t block, uint32_t count);
static int journal_commit(void);
static int journal_replay(void);

/* --- Functions ---*/

/* ~~~ Level 4: Internal ~~~ */

/* Block I/O */

static int read_block(uint64_t block, void *buf) {
	uint8_t sectors = SMKFS_BLOCK_SIZE / SMKFS_SECTOR_SIZE;
	uint32_t lba = (uint32_t)(block * sectors);
	uint8_t *ptr = (uint8_t *)buf;

	for (uint8_t i = 0; i < sectors; i++) {
		if (ide_read_sectors(drive_num, lba, 1, (uint16_t *)(ptr + i * SMKFS_SECTOR_SIZE)) != 0) {
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
		if (ide_write_sectors(drive_num, lba, 1, (const uint16_t *)(ptr + i * SMKFS_SECTOR_SIZE)) != 0) {
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
	uint32_t sum = 0xFFFFFFFF;

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
	uint8_t tmp_buf[SMKFS_BLOCK_SIZE];

	if (len > sizeof(tmp_buf)) {
		return -1;
	}

	memcpy(tmp_buf, data, len);
	((smkfs_header_t *)tmp_buf)->checksum = 0;
	uint32_t computed = checksum_compute(tmp_buf, len);

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
	if (read_block(bitmap_block, buf) != 0) return;
	if (((buf[block_offset] >> bit_offset) & 1) == 0) {
		sb.free_blocks--;
	}
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
	if (read_block(bitmap_block, buf) != 0) return;
	if (((buf[block_offset] >> bit_offset) & 1) != 0) {
		sb.free_blocks++;
	}
	buf[block_offset] &= ~(1 << bit_offset);
	write_block(bitmap_block, buf);
}

static int SMKFS_UNUSED bitmap_test(uint64_t block) {
	if (block < sb.data_start) return -1;
	uint64_t idx = block - sb.data_start;
	uint64_t byte_offset = idx / 8;
	uint64_t bit_offset = idx % 8;
	uint64_t bitmap_block = sb.bitmap_start + (byte_offset / SMKFS_BLOCK_SIZE);
	uint64_t block_offset = byte_offset % SMKFS_BLOCK_SIZE;

	uint8_t buf[SMKFS_BLOCK_SIZE];
	if (read_block(bitmap_block, buf) != 0) return -1;
	return (buf[block_offset] >> bit_offset) & 1;
}

/*
 * TODO:
 * optimize
*/
static uint64_t bitmap_alloc_range(uint32_t count) {
	if (count == 0) return -1;

	uint64_t total_bits = sb.total_blocks - sb.data_start;
	uint64_t run_start = 0;
	uint32_t run_len = 0;

	for (uint64_t bb = 0; bb < sb.bitmap_length; bb++) {
		uint8_t buf[SMKFS_BLOCK_SIZE];
		if (read_block(sb.bitmap_start + bb, buf) != 0) return -1;

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

/* Record */

static int record_read(uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size) {
	uint8_t block[SMKFS_BLOCK_SIZE];
	if (!rec || !attr_buf) return -1;
	if (read_block(record_id, block) != 0) return -1;

	memcpy(rec, block, sizeof(smkfs_record_t));

	if (header_validate(&rec->header, SMKFS_ST_RECORD) != 0) return -1;
	if (rec->header.length < sizeof(smkfs_record_t)) return -1;
	if (header_checksum_verify(&rec->header, block, rec->header.length) != 0) return -1;

	size_t attr_len = rec->header.length - sizeof(smkfs_record_t);
	if (attr_len > buf_size) attr_len = buf_size;

	memcpy(attr_buf, block + sizeof(smkfs_record_t), attr_len);
	return (int)attr_len;
}

static int record_write(uint64_t record_id, const smkfs_record_t *rec, const void *attr_buf) {
	uint8_t block[SMKFS_BLOCK_SIZE];
	size_t total_len = 0;

	if (!rec) return -1;
	total_len = rec->header.length;
	if (total_len < sizeof(smkfs_record_t) || total_len > SMKFS_BLOCK_SIZE) return -1;
	if (total_len > sizeof(smkfs_record_t) && !attr_buf) return -1;

	memset(block, 0, sizeof(block));
	memcpy(block, rec, sizeof(smkfs_record_t));
	memcpy(block + sizeof(smkfs_record_t), attr_buf, total_len - sizeof(smkfs_record_t));

	header_checksum_update(&((smkfs_record_t *)block)->header, block, total_len);

	return write_block(record_id, block);
}

static uint64_t record_alloc(uint16_t object_type) {
	uint64_t block = bitmap_alloc();
	if (block <= 0) return 0;

	smkfs_record_t rec;
	header_init(&rec.header, SMKFS_ST_RECORD, sizeof(smkfs_record_t) + sizeof(smkfs_attr_header_t), 0);
	rec.record_id = block;
	rec.object_type = object_type;
	rec.attr_count = 0;

	smkfs_attr_header_t term;
	term.type = SMKFS_ATTRT_END;
	term.flags = 0;
	term.id = 0;
	term.length = 0;
	record_write(block, &rec, &term);

	sb.record_count++;
	return block;
}

static void record_free(uint64_t record_id) {
	extent_remove_all(record_id);

	uint8_t block[SMKFS_BLOCK_SIZE];
	if (read_block(record_id, block) == 0) {
		smkfs_record_t *rec = (smkfs_record_t *)block;
		rec->header.flags |= SMKFS_FLA_DELETED;
		header_checksum_update(&rec->header, block, rec->header.length);
		write_block(record_id, block);
	}

	bitmap_clear(record_id);
	sb.record_count--;
}

static int record_find_attr(const void *attr_buf, uint16_t attr_type, void **out_attr, size_t *out_len) {
	const uint8_t *ptr = (const uint8_t *)attr_buf;

	while (1) {
		smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
		if (ah->type == SMKFS_ATTRT_END) break;
		if (ah->type == attr_type) {
			if (out_attr) *out_attr = (void *)(ptr + sizeof(smkfs_attr_header_t));
			if (out_len) *out_len = ah->length;
			return 0;
		}
		ptr += sizeof(smkfs_attr_header_t) + ah->length;
	}
	return -1;
}

static int record_add_attr(void *attr_buf, size_t buf_size, uint16_t attr_type, const void *data, size_t data_len) {
	uint8_t *ptr = (uint8_t *)attr_buf;
	size_t used = 0;

	if (!attr_buf || (data_len > 0 && !data)) return -1;
	if (buf_size < 2 * sizeof(smkfs_attr_header_t) + data_len) return -1;

	record_remove_attr(attr_buf, attr_type);

	ptr = (uint8_t *)attr_buf;
	while (1) {
		smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        if (ah->type == SMKFS_ATTRT_END) {
            used = (size_t)(ptr - (uint8_t *)attr_buf) + sizeof(smkfs_attr_header_t);
            break;
		}

		ptr += sizeof(smkfs_attr_header_t) + ah->length;
	}

	size_t need = sizeof(smkfs_attr_header_t) + data_len + sizeof(smkfs_attr_header_t);
    if (used + need > buf_size) return -1;

	smkfs_attr_header_t *new_ah = (smkfs_attr_header_t *)(ptr);
    new_ah->type = attr_type;
    new_ah->flags = 0;
    new_ah->id = 0;
    new_ah->length = (uint32_t)data_len;
    memcpy(ptr + sizeof(smkfs_attr_header_t), data, data_len);

    smkfs_attr_header_t *term = (smkfs_attr_header_t *)(ptr + sizeof(smkfs_attr_header_t) + data_len);
    term->type = SMKFS_ATTRT_END;
    term->flags = 0;
    term->id = 0;
    term->length = 0;

	return 0;
}

static int record_remove_attr(void *attr_buf, uint16_t attr_type) {
    uint8_t *ptr = (uint8_t *)attr_buf;
    uint8_t *found = NULL;
    size_t found_len = 0;

    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        if (ah->type == SMKFS_ATTRT_END) break;
        if (ah->type == attr_type) {
            found = ptr;
            found_len = sizeof(smkfs_attr_header_t) + ah->length;
        }
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }

    if (!found) return 0;

    size_t tail = (size_t)(ptr - (found + found_len));
    memmove(found, found + found_len, tail);
    return 0;
}

/* Extent */

static int SMKFS_UNUSED extent_resolve(uint64_t record_id, uint64_t logical_block, smkfs_extent_t *out) {
	uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
	smkfs_record_t rec;
	int attr_len = record_read(record_id, &rec, attr_buf, sizeof(attr_buf));
	if (attr_len < 0) return -1;

	void *attr_data;
	size_t attr_data_len = 0;
	if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &attr_data, &attr_data_len) != 0) {
		return -1;
	}

	uint32_t num_extents = attr_data_len / sizeof(smkfs_extent_t);
	smkfs_extent_t *extents = (smkfs_extent_t *)attr_data;

	for (uint32_t i = 0; i < num_extents; i++) {
		if (logical_block >= extents[i].logical_offset && logical_block < extents[i].logical_offset + extents[i].block_count) {
			if (out) *out = extents[i];
			return 0;
		}
	}
	return -1;
}

/*
 * TODO:
 * optimize
*/
static int SMKFS_UNUSED extent_add(uint64_t record_id, uint64_t logical_block,
        uint64_t physical_block, uint32_t count) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_record_t *rec = (smkfs_record_t *)block;

    if (read_block(record_id, block) != 0) return -1;

    uint8_t *attr_buf = block + sizeof(smkfs_record_t);
    size_t attr_space = SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t);

    void *existing;
    size_t existing_len = 0;
    smkfs_extent_t extents[32];
    uint32_t num_extents = 0;

    if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &existing, &existing_len) == 0) {
        num_extents = existing_len / sizeof(smkfs_extent_t);
        if (num_extents >= 32) return -1;
        memcpy(extents, existing, existing_len);
    }

    extents[num_extents].logical_offset = logical_block;
    extents[num_extents].physical_block = physical_block;
    extents[num_extents].block_count = count;
    num_extents++;

    record_remove_attr(attr_buf, SMKFS_ATTRT_EXTENTS);
    if (record_add_attr(attr_buf, attr_space, SMKFS_ATTRT_EXTENTS, extents, num_extents * sizeof(smkfs_extent_t)) != 0) {
        return -1;
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
    return write_block(record_id, block);
}

static void extent_remove_all(uint64_t record_id) {
	uint8_t block[SMKFS_BLOCK_SIZE];
	smkfs_record_t *rec = (smkfs_record_t *)block;

	if (read_block(record_id, block) != 0) return;

	uint8_t *attr_buf = block + sizeof(smkfs_record_t);

	void *existing;
	size_t existing_len = 0;
	if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &existing, &existing_len) != 0) {
		return;
	}

	uint32_t num_extents = existing_len / sizeof(smkfs_extent_t);
	smkfs_extent_t *extents = (smkfs_extent_t *)existing;

	for (uint32_t i = 0; i < num_extents; i++) {
        bitmap_free_range(extents[i].physical_block, extents[i].block_count);
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
    write_block(record_id, block);
}

/* B+ tree */

static int btree_node_read(uint64_t block, smkfs_btree_node_t *node, void *payload, size_t payload_size) {
	uint8_t raw[SMKFS_BLOCK_SIZE];
	if (!node || block == 0) return -1;
	if (payload_size > 0 && !payload) return -1;
	if (read_block(block, raw) != 0) return -1;
	memcpy(node, raw, sizeof(*node));
	if (header_validate(&node->header, SMKFS_ST_BTREE_NODE) != 0) return -1;
	if (header_checksum_verify(&node->header, raw, node->header.length) != 0) return -1;
	if (node->header.length < sizeof(*node)) return -1;
	if (node->header.length - sizeof(*node) > payload_size) return -1;
	memcpy(payload, raw + sizeof(*node), node->header.length - sizeof(*node));
	return 0;
}

static int btree_node_write(uint64_t block, const smkfs_btree_node_t *node, const void *payload, size_t payload_size) {
	uint8_t raw[SMKFS_BLOCK_SIZE];
	if (!node || block == 0) return -1;
	if (payload_size > 0 && !payload) return -1;
	memset(raw, 0, sizeof(raw));
	memcpy(raw, node, sizeof(*node));
	if (payload_size > 0) {
		memcpy(raw + sizeof(*node), payload, payload_size);
	}
	((smkfs_btree_node_t *)raw)->header.length = sizeof(*node) + payload_size;
	header_checksum_update(&((smkfs_btree_node_t *)raw)->header, raw, ((smkfs_btree_node_t *)raw)->header.length);
	return write_block(block, raw);
}

static int btree_leaf_insert_entry(smkfs_btree_node_t *node, smkfs_btree_leaf_entry_t *entries, uint32_t *count, const char *key, uint64_t value) {
	uint32_t pos = 0;
	uint32_t max_entries = (uint32_t)((SMKFS_BLOCK_SIZE - sizeof(smkfs_btree_node_t)) / sizeof(smkfs_btree_leaf_entry_t));

	if (!node || !entries || !count || !key) return -1;
	for (uint32_t i = 0; i < *count; i++) {
		int cmp = strcmp(entries[i].name, key);
		if (cmp == 0) {
			entries[i].record_id = value;
			return 0;
		}
		if (cmp > 0) {
			pos = i;
			break;
		}
		pos = i + 1;
	}
	if (*count >= max_entries) return 1;
	memmove(&entries[pos + 1], &entries[pos], ((*count - pos) * sizeof(smkfs_btree_leaf_entry_t)));
	memset(&entries[pos], 0, sizeof(smkfs_btree_leaf_entry_t));
	strncpy(entries[pos].name, key, SMKFS_NAME_LEN - 1);
	entries[pos].record_id = value;
	(*count)++;
	return 0;
}

static int btree_leaf_remove_entry(smkfs_btree_node_t *node, smkfs_btree_leaf_entry_t *entries, uint32_t *count, const char *key) {
	uint32_t index = 0;

	if (!node || !entries || !count || !key) return -1;
	for (uint32_t i = 0; i < *count; i++) {
		if (strcmp(entries[i].name, key) == 0) {
			index = i;
			break;
		}
	}
	if (index >= *count) return -1;
	memmove(&entries[index], &entries[index + 1], ((*count - index - 1) * sizeof(smkfs_btree_leaf_entry_t)));
	memset(&entries[*count - 1], 0, sizeof(smkfs_btree_leaf_entry_t));
	(*count)--;
	return 0;
}

static int btree_insert_recursive(uint64_t block, const char *key, uint64_t value, uint64_t *new_root) {
	smkfs_btree_node_t node;
	smkfs_btree_leaf_entry_t entries[64];
	uint32_t count = 0;
	uint64_t new_leaf = 0;
	uint64_t next_sibling = 0;
	uint32_t split_point = 0;

	if (!key || !new_root || block == 0) return -1;
	if (btree_node_read(block, &node, entries, sizeof(entries)) != 0) return -1;
	if (!(node.flags & SMKFS_BTN_LEAF)) return -1;
	count = node.key_count;
	if (btree_leaf_insert_entry(&node, entries, &count, key, value) == 1) {
		new_leaf = bitmap_alloc();
		if (new_leaf <= 0) return -1;
		split_point = count / 2;
		next_sibling = node.right_sibling;
		node.right_sibling = new_leaf;
		node.key_count = split_point;
		if (btree_node_write(block, &node, entries, node.key_count * sizeof(smkfs_btree_leaf_entry_t)) != 0) return -1;

		memset(&node, 0, sizeof(node));
		header_init(&node.header, SMKFS_ST_BTREE_NODE, sizeof(node), SMKFS_BTN_LEAF);
		node.parent_block = 0;
		node.flags = SMKFS_BTN_LEAF;
		node.key_count = count - split_point;
		node.right_sibling = next_sibling;
		if (btree_node_write(new_leaf, &node, &entries[split_point], node.key_count * sizeof(smkfs_btree_leaf_entry_t)) != 0) return -1;
		*new_root = block;
		return 0;
	}

	node.key_count = count;
	return btree_node_write(block, &node, entries, count * sizeof(smkfs_btree_leaf_entry_t));
}


static int btree_search(uint64_t root_block, const char *key, uint64_t *out_value) {
	uint8_t block[SMKFS_BLOCK_SIZE];
	smkfs_btree_node_t node;
	smkfs_btree_leaf_entry_t entries[64];
	smkfs_btree_index_entry_t index_entries[64];

	if (!key || !out_value || root_block == 0) return -1;
	if (read_block(root_block, block) != 0) return -1;
	memcpy(&node, block, sizeof(node));
	if (header_validate(&node.header, SMKFS_ST_BTREE_NODE) != 0) return -1;
	if (header_checksum_verify(&node.header, block, node.header.length) != 0) return -1;
	if (node.flags & SMKFS_BTN_LEAF) {
		memcpy(entries, block + sizeof(node), node.key_count * sizeof(smkfs_btree_leaf_entry_t));
		for (uint32_t i = 0; i < node.key_count; i++) {
			if (strcmp(entries[i].name, key) == 0) {
				*out_value = entries[i].record_id;
				return 0;
			}
		}
		if (node.right_sibling != 0) {
			return btree_search(node.right_sibling, key, out_value);
		}
		return -1;
	}

	memcpy(index_entries, block + sizeof(node), node.key_count * sizeof(smkfs_btree_index_entry_t));
	for (uint32_t i = 0; i < node.key_count; i++) {
		if (strcmp(key, index_entries[i].prefix) < 0) {
			return btree_search(index_entries[i].child_block, key, out_value);
		}
	}
	return btree_search(index_entries[node.key_count - 1].child_block, key, out_value);
}

static int btree_insert(uint64_t root_block, const char *key, uint64_t value, uint64_t *new_root) {
	uint64_t new_block = 0;
	smkfs_btree_node_t node;
	smkfs_btree_leaf_entry_t entries[64];

	if (!key || !new_root) return -1;
	if (root_block == 0) {
		new_block = bitmap_alloc();
		if (new_block <= 0) return -1;
		memset(&node, 0, sizeof(node));
		header_init(&node.header, SMKFS_ST_BTREE_NODE, sizeof(node), SMKFS_BTN_LEAF | SMKFS_BTN_ROOT);
		node.parent_block = 0;
		node.flags = SMKFS_BTN_LEAF | SMKFS_BTN_ROOT;
		node.key_count = 1;
		memset(entries, 0, sizeof(entries));
		strncpy(entries[0].name, key, SMKFS_NAME_LEN - 1);
		entries[0].record_id = value;
		if (btree_node_write(new_block, &node, entries, sizeof(smkfs_btree_leaf_entry_t)) != 0) return -1;
		*new_root = new_block;
		return 0;
	}
	return btree_insert_recursive(root_block, key, value, new_root);
}

static int btree_delete(uint64_t root_block, const char *key, uint64_t *new_root) {
	smkfs_btree_node_t node;
	smkfs_btree_leaf_entry_t entries[64];
	uint32_t count = 0;

	if (!key || !new_root || root_block == 0) return -1;
	if (btree_node_read(root_block, &node, entries, sizeof(entries)) != 0) return -1;
	count = node.key_count;
	if (btree_leaf_remove_entry(&node, entries, &count, key) != 0) return -1;
	node.key_count = count;
	if (btree_node_write(root_block, &node, entries, count * sizeof(smkfs_btree_leaf_entry_t)) != 0) return -1;
	*new_root = root_block;
	return 0;
}

static int btree_iterate(uint64_t root_block, int (*cb)(const char *key, uint64_t value, void *ctx), void *ctx) {
	uint8_t block[SMKFS_BLOCK_SIZE];
	smkfs_btree_node_t node;
	smkfs_btree_leaf_entry_t entries[64];

	if (!cb || !ctx || root_block == 0) return -1;
	if (read_block(root_block, block) != 0) return -1;
	memcpy(&node, block, sizeof(node));
	if (header_validate(&node.header, SMKFS_ST_BTREE_NODE) != 0) return -1;
	if (header_checksum_verify(&node.header, block, node.header.length) != 0) return -1;
	memcpy(entries, block + sizeof(node), node.key_count * sizeof(smkfs_btree_leaf_entry_t));
	for (uint32_t i = 0; i < node.key_count; i++) {
		if (cb(entries[i].name, entries[i].record_id, ctx) != 0) return -1;
	}
	if (node.right_sibling != 0) {
		return btree_iterate(node.right_sibling, cb, ctx);
	}
	return 0;
}

/* Journal */

static int SMKFS_UNUSED journal_start_transaction(void) {
	if (journal_in_transaction) return -1;
	journal_in_transaction = 1;
	return (int)journal_next_sequence;
}

static int SMKFS_UNUSED journal_log_write(uint64_t block, const void *old_data, const void *new_data, size_t len) {
	uint8_t buf[SMKFS_BLOCK_SIZE];
	smkfs_journal_entry_t *ent;
	size_t total_len;

	if (!journal_in_transaction || len > SMKFS_BLOCK_SIZE) return -1;
	if (len > 0 && (!old_data || !new_data)) return -1;

	total_len = sizeof(smkfs_journal_entry_t) + len * 2;
    if (total_len > SMKFS_BLOCK_SIZE) return -1;

    memset(buf, 0, sizeof(buf));
    ent = (smkfs_journal_entry_t *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT,
            sizeof(smkfs_journal_entry_t) + len * 2, 0);
    ent->sequence = journal_next_sequence++;
    ent->target_block = block;
    ent->operation = SMKFS_JOP_WRITE;
    ent->data_length = (uint32_t)(len * 2);

    if (len > 0) {
        memcpy(buf + sizeof(smkfs_journal_entry_t), old_data, len);
        memcpy(buf + sizeof(smkfs_journal_entry_t) + len, new_data, len);
    }

	header_checksum_update(&ent->header, buf, ent->header.length);

    if (write_block(sb.journal_start + journal_write_pos, buf) != 0)
        return -1;

    journal_write_pos++;
    if (journal_write_pos >= sb.journal_length) {
        journal_write_pos = 0;
	}
	return 0;
}

static int SMKFS_UNUSED journal_log_alloc(uint64_t block, uint32_t count) {
	uint8_t buf[SMKFS_BLOCK_SIZE];
    smkfs_journal_entry_t *ent;

    if (!journal_in_transaction) return -1;

    memset(buf, 0, sizeof(buf));
    ent = (smkfs_journal_entry_t *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(smkfs_journal_entry_t), 0);
    ent->sequence = journal_next_sequence++;
    ent->target_block = block;
    ent->operation = SMKFS_JOP_ALLOC;
    ent->data_length = count;

    header_checksum_update(&ent->header, buf, ent->header.length);

    if (write_block(sb.journal_start + journal_write_pos, buf) != 0) {
        return -1;
	}

    journal_write_pos++;
    if (journal_write_pos >= sb.journal_length) {
        journal_write_pos = 0;
	}
	return 0;
}

static int SMKFS_UNUSED journal_log_free(uint64_t block, uint32_t count) {
	uint8_t buf[SMKFS_BLOCK_SIZE];
    smkfs_journal_entry_t *ent;

    if (!journal_in_transaction) return -1;

    memset(buf, 0, sizeof(buf));
    ent = (smkfs_journal_entry_t *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(smkfs_journal_entry_t), 0);
    ent->sequence = journal_next_sequence++;
    ent->target_block = block;
    ent->operation = SMKFS_JOP_FREE;
    ent->data_length = count;

    header_checksum_update(&ent->header, buf, ent->header.length);

    if (write_block(sb.journal_start + journal_write_pos, buf) != 0) {
        return -1;
	}

    journal_write_pos++;
    if (journal_write_pos >= sb.journal_length) {
        journal_write_pos = 0;
	}

	return 0;
}

static int SMKFS_UNUSED journal_commit(void) {
	uint8_t buf[SMKFS_BLOCK_SIZE];
    smkfs_journal_entry_t *ent;

    if (!journal_in_transaction) return -1;

    memset(buf, 0, sizeof(buf));
    ent = (smkfs_journal_entry_t *)buf;
    header_init(&ent->header, SMKFS_ST_JOURNAL_ENT, sizeof(smkfs_journal_entry_t), 0);
    ent->sequence = journal_next_sequence++;
    ent->target_block = 0;
    ent->operation = SMKFS_JOP_COMMIT;
    ent->data_length = 0;

    header_checksum_update(&ent->header, buf, ent->header.length);

    if (write_block(sb.journal_start + journal_write_pos, buf) != 0) {
        return -1;
	}

    journal_write_pos++;
    if (journal_write_pos >= sb.journal_length) {
        journal_write_pos = 0;
	}

    journal_in_transaction = 0;
    return 0;
}
static int journal_replay(void) {
    uint8_t buf[SMKFS_BLOCK_SIZE];
    smkfs_journal_entry_t *ent;
    uint64_t replay_limit = sb.journal_length;
    int committed = 0;

    for (uint64_t i = 0; i < sb.journal_length; i++) {
        if (read_block(sb.journal_start + i, buf) != 0) continue;

        ent = (smkfs_journal_entry_t *)buf;
        if (header_validate(&ent->header, SMKFS_ST_JOURNAL_ENT) != 0)
            continue;
        if (header_checksum_verify(&ent->header, buf, ent->header.length) != 0)
            continue;

        if (ent->operation == SMKFS_JOP_COMMIT) {
            replay_limit = i;
            committed = 1;
            break;
        }
    }

    if (!committed) {
        for (uint64_t i = 0; i < sb.journal_length; i++) {
            memset(buf, 0, sizeof(buf));
            write_block(sb.journal_start + i, buf);
        }
        journal_write_pos = 0;
        return 0;
    }

    for (uint64_t i = 0; i < replay_limit; i++) {
        if (read_block(sb.journal_start + i, buf) != 0) continue;

        ent = (smkfs_journal_entry_t *)buf;
        if (header_validate(&ent->header, SMKFS_ST_JOURNAL_ENT) != 0)
            break;
        if (header_checksum_verify(&ent->header, buf, ent->header.length) != 0)
            break;

        if (ent->operation == SMKFS_JOP_WRITE) {
            uint8_t new_data[SMKFS_BLOCK_SIZE];
            size_t half = ent->data_length / 2;
            memcpy(new_data, buf + sizeof(smkfs_journal_entry_t) + half, half);
            write_block(ent->target_block, new_data);
        } else if (ent->operation == SMKFS_JOP_ALLOC) {
            for (uint32_t j = 0; j < ent->data_length; j++)
                bitmap_set(ent->target_block + j);
        } else if (ent->operation == SMKFS_JOP_FREE) {
            for (uint32_t j = 0; j < ent->data_length; j++)
                bitmap_clear(ent->target_block + j);
        }
    }

    for (uint64_t i = 0; i < sb.journal_length; i++) {
        memset(buf, 0, sizeof(buf));
        write_block(sb.journal_start + i, buf);
    }

    journal_write_pos = 0;
    return 0;
}

/* ~~~ Level 3: Kernel ~~~ */
int smkfs_mount(uint8_t drive) {
	uint8_t block[SMKFS_BLOCK_SIZE];
	if (mounted) return -1;

	drive_num = drive;
	if (read_block(0, block) != 0) {
		printk("[SmKFS] Failed to read superblock\n");
		return -1;
	}

	memcpy(&sb, block, sizeof(sb));

	if (header_validate(&sb.header, SMKFS_ST_SUPERBLOCK) != 0) {
		printk("[SmKFS] Superblock header invalid\n");
		return -1;
	}

	if (header_checksum_verify(&sb.header, block, sb.header.length) != 0) {
		printk("[SmKFS] Superblock checksum mismatch\n");
		return -1;
	}

	journal_replay();

	mounted = 1;
    printk("[SmKFS] Mounted drive %d, %llu blocks, %llu free\n", drive, sb.total_blocks, sb.free_blocks);

	return 0;
}

int smkfs_unmount(void) {
	uint8_t block[SMKFS_BLOCK_SIZE];

	if (!mounted) return -1;

	memset(block, 0, sizeof(block));
    memcpy(block, &sb, sizeof(sb));
    header_checksum_update(&((smkfs_superblock_t *)block)->header,
            block, sizeof(smkfs_superblock_t));

    if (write_block(0, block) != 0) {
        printk("[SmKFS] Failed to write superblock\n");
        return -1;
    }

    mounted = 0;
    printk("[SmKFS] Unmounted\n");
    return 0;
}

int smkfs_sync(void) {
	uint8_t block[SMKFS_BLOCK_SIZE];

	if (!mounted) return -1;

	memset(block, 0, sizeof(block));
	memcpy(block, &sb, sizeof(sb));
	header_checksum_update(&((smkfs_superblock_t *)block)->header, block, sizeof(smkfs_superblock_t));

	if (write_block(0, block) != 0) {
		printk("[SmKFS] Sync failed\n");
		return -1;
	}

	printk("[SmKFS] Synced\n");
	return 0;
}

int smkfs_lookup_by_name(uint64_t dir_record, const char *name, uint64_t *out_record) {
	uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *root_block;

    if (!mounted || !name || !out_record) return -1;

    if (record_read(dir_record, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return -1;
	}

	if (rec.object_type != SMKFS_ROT_DIR) return -1;

    if (record_find_attr(attr_buf, SMKFS_ATTRT_NAME, (void **)&root_block, NULL) != 0) {
        return -1;
    }

    return btree_search(*root_block, name, out_record);
}

int smkfs_create_record(uint16_t object_type, uint64_t parent_dir, const char *name, uint64_t *out_record) {
    uint8_t parent_attr[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t parent_rec;
    uint64_t new_id;
    uint64_t btree_root = 0;
    uint64_t *parent_btree;

    if (!mounted || !name || !out_record) return -1;

    new_id = record_alloc(object_type);
    if (new_id == 0) return -1;

    if (object_type == SMKFS_ROT_DIR) {
        uint8_t empty_leaf[SMKFS_BLOCK_SIZE];
        smkfs_btree_node_t *node;

        memset(empty_leaf, 0, sizeof(empty_leaf));
        node = (smkfs_btree_node_t *)empty_leaf;
        header_init(&node->header, SMKFS_ST_BTREE_NODE, sizeof(smkfs_btree_node_t), SMKFS_BTN_LEAF | SMKFS_BTN_ROOT);
        node->parent_block = 0;
        node->key_count = 0;
        node->right_sibling = 0;
        header_checksum_update(&node->header, empty_leaf, sizeof(smkfs_btree_node_t));

        btree_root = bitmap_alloc();
        if (btree_root <= 0) {
            record_free(new_id);
            return -1;
        }
        if (write_block(btree_root, empty_leaf) != 0) {
            bitmap_clear(btree_root);
            record_free(new_id);
            return -1;
        }
    }

    if (record_read(parent_dir, &parent_rec, parent_attr, sizeof(parent_attr)) < 0) {
        if (btree_root) bitmap_clear(btree_root);
        record_free(new_id);
        return -1;
    }

    if (record_find_attr(parent_attr, SMKFS_ATTRT_DATA, (void **)&parent_btree, NULL) != 0) {
        if (btree_root) bitmap_clear(btree_root);
        record_free(new_id);
        return -1;
    }

    uint64_t new_root = *parent_btree;
    if (btree_insert(*parent_btree, name, new_id, &new_root) != 0) {
        if (btree_root) bitmap_clear(btree_root);
        record_free(new_id);
        return -1;
    }

    uint8_t new_attr[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    memset(new_attr, 0, sizeof(new_attr));
    smkfs_attr_header_t term;
    term.type = SMKFS_ATTRT_END;
    term.flags = 0;
    term.id = 0;
    term.length = 0;
    memcpy(new_attr, &term, sizeof(term));

    record_add_attr(new_attr, sizeof(new_attr), SMKFS_ATTRT_NAME, name, strlen(name) + 1);
    record_add_attr(new_attr, sizeof(new_attr), SMKFS_ATTRT_PARENT, &parent_dir, sizeof(parent_dir));
    if (btree_root) {
        record_add_attr(new_attr, sizeof(new_attr), SMKFS_ATTRT_DATA, &btree_root, sizeof(btree_root));
    }

    smkfs_record_t new_rec;
    header_init(&new_rec.header, SMKFS_ST_RECORD, sizeof(smkfs_record_t), 0);
    new_rec.record_id = new_id;
    new_rec.object_type = object_type;
    new_rec.attr_count = btree_root ? 3 : 2;

    if (record_write(new_id, &new_rec, new_attr) != 0) {
        if (btree_root) bitmap_clear(btree_root);
        record_free(new_id);
        return -1;
    }

    parent_rec.attr_count++;
    record_add_attr(parent_attr, sizeof(parent_attr), SMKFS_ATTRT_DATA, &new_root, sizeof(new_root));
    if (record_write(parent_dir, &parent_rec, parent_attr) != 0) {
        record_free(new_id);
        return -1;
    }

    *out_record = new_id;
    return 0;
}
int smkfs_delete_record(uint64_t record_id) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    char *name;
    uint64_t *parent_ptr;
    uint64_t parent_dir;
    uint64_t *parent_btree;
    uint8_t parent_attr[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t parent_rec;
    uint64_t new_root;

    if (!mounted || record_id == 0) return -1;

    if (record_read(record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return -1;
	}

    if (rec.object_type == SMKFS_ROT_DIR) {
        uint64_t *dir_btree;
        if (record_find_attr(attr_buf, SMKFS_ATTRT_DATA, (void **)&dir_btree, NULL) == 0) {
            smkfs_btree_node_t node;
            smkfs_btree_leaf_entry_t entries[64];
            if (btree_node_read(*dir_btree, &node, entries, sizeof(entries)) == 0) {
                if (node.key_count > 0) {
                    printk("[SmKFS] Directory not empty\n");
                    return -1;
                }
            }
        }
    }

    if (record_find_attr(attr_buf, SMKFS_ATTRT_NAME, (void **)&name, NULL) != 0) {
        return -1;
	}

    if (record_find_attr(attr_buf, SMKFS_ATTRT_PARENT, (void **)&parent_ptr, NULL) != 0) {
        return -1;
	}
    parent_dir = *parent_ptr;

    if (record_read(parent_dir, &parent_rec, parent_attr,sizeof(parent_attr)) < 0) {
        return -1;
	}

    if (record_find_attr(parent_attr, SMKFS_ATTRT_DATA, (void **)&parent_btree, NULL) != 0) {
        return -1;
	}

    new_root = *parent_btree;
    if (btree_delete(*parent_btree, name, &new_root) != 0) {
        return -1;
	}

    record_free(record_id);

    parent_rec.attr_count++;
    record_add_attr(parent_attr, sizeof(parent_attr), SMKFS_ATTRT_DATA, &new_root, sizeof(new_root));
    record_write(parent_dir, &parent_rec, parent_attr);

    return 0;
}

int smkfs_rename(uint64_t record_id, uint64_t new_parent, const char *new_name) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    char *old_name;
    uint64_t *old_parent_ptr;
    uint64_t old_parent;
    uint8_t old_parent_attr[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t old_parent_rec;
    uint64_t *old_btree;
    uint64_t new_root;
    uint8_t new_parent_attr[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t new_parent_rec;
    uint64_t *new_btree;
    uint64_t new_root2;

    if (!mounted || !new_name || record_id == 0 || new_parent == 0) {
        return -1;
	}

    if (record_read(record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return -1;
	}

    if (record_find_attr(attr_buf, SMKFS_ATTRT_NAME, (void **)&old_name, NULL) != 0) {
        return -1;
	}

    if (record_find_attr(attr_buf, SMKFS_ATTRT_PARENT, (void **)&old_parent_ptr, NULL) != 0) {
        return -1;
	}
    old_parent = *old_parent_ptr;

    if (record_read(old_parent, &old_parent_rec, old_parent_attr, sizeof(old_parent_attr)) < 0) {
        return -1;
	}

    if (record_find_attr(old_parent_attr, SMKFS_ATTRT_DATA, (void **)&old_btree, NULL) != 0) {
        return -1;
	}

    new_root = *old_btree;
    if (btree_delete(*old_btree, old_name, &new_root) != 0) {
        return -1;
	}

    record_add_attr(old_parent_attr, sizeof(old_parent_attr), SMKFS_ATTRT_DATA, &new_root, sizeof(new_root));
    if (record_write(old_parent, &old_parent_rec, old_parent_attr) != 0) {
        return -1;
	}

    if (record_read(new_parent, &new_parent_rec, new_parent_attr, sizeof(new_parent_attr)) < 0) {
        return -1;
	}

    if (record_find_attr(new_parent_attr, SMKFS_ATTRT_DATA, (void **)&new_btree, NULL) != 0) {
        return -1;
	}

    new_root2 = *new_btree;
    if (btree_insert(*new_btree, new_name, record_id, &new_root2) != 0) {
        return -1;
	}

    record_add_attr(new_parent_attr, sizeof(new_parent_attr), SMKFS_ATTRT_DATA, &new_root2, sizeof(new_root2));
    if (record_write(new_parent, &new_parent_rec, new_parent_attr) != 0) {
        return -1;
	}

    record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_NAME, new_name, strlen(new_name) + 1);
    record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_PARENT, &new_parent, sizeof(new_parent));
    if (record_write(record_id, &rec, attr_buf) != 0) {
		return -1;
	}

    return 0;
}

int smkfs_read(uint64_t record_id, uint64_t offset, size_t len, void *buf) {
	uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
	smkfs_record_t rec;
	uint64_t *fsize_ptr;
	uint64_t file_size;
	size_t to_read;
	uint8_t *out = (uint8_t *)buf;

	if (!mounted || !buf || record_id == 0) return -1;

	if (record_read(record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
		return -1;
	}

	if (rec.object_type != SMKFS_ROT_FILE) return -1;

	if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (void **)&fsize_ptr, NULL) != 0) {
		return -1;
	}

	file_size = *fsize_ptr;

	if (offset >= file_size) return 0; // not fail, literally reading 0 bytes
	to_read = len;
	if (offset + to_read > file_size) {
		to_read = (size_t)(file_size - offset);
	}

	for (size_t done = 0; done < to_read; ) {
		uint64_t logical_block = (offset + done) / SMKFS_BLOCK_SIZE;
		uint64_t block_offset = (offset + done) % SMKFS_BLOCK_SIZE;
		smkfs_extent_t ext;
		uint8_t block[SMKFS_BLOCK_SIZE];
		size_t chunk;

		if (extent_resolve(record_id, logical_block, &ext) != 0) {
			return -1;
		}

		if (read_block(ext.physical_block + (logical_block - ext.logical_offset), block) != 0) {
			return -1;
		}

		chunk = to_read - done;
		if (chunk > SMKFS_BLOCK_SIZE - block_offset) {
			chunk = SMKFS_BLOCK_SIZE - block_offset;
		}

		memcpy(out + done, block + block_offset, chunk);
		done += chunk;
	}
	return (int)to_read;
}

int smkfs_write(uint64_t record_id, uint64_t offset, size_t len, const void *buf) {
	uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
	smkfs_record_t rec;
	uint64_t *fsize_ptr;
	uint64_t file_size;
	uint64_t new_size;
	const uint8_t *in = (uint8_t *)buf;

	if (!mounted || !buf || record_id == 0) return -1;

	if (record_read(record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
		return -1;
	}

	if (rec.object_type != SMKFS_ROT_FILE) return -1;

	if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (void **)&fsize_ptr, NULL) != 0) {
		file_size = 0;
	} else {
		file_size = *fsize_ptr;
	}

	new_size = offset + len;
	if (new_size < file_size) new_size = file_size;

	for (size_t done = 0; done < len; ) {
        uint64_t logical_block = (offset + done) / SMKFS_BLOCK_SIZE;
        uint64_t block_offset = (offset + done) % SMKFS_BLOCK_SIZE;
        smkfs_extent_t ext;
        uint8_t block[SMKFS_BLOCK_SIZE];
        size_t chunk;
        uint64_t phys_block;

		if (extent_resolve(record_id, logical_block, &ext) == 0) {
            phys_block = ext.physical_block + (logical_block - ext.logical_offset);
            if (read_block(phys_block, block) != 0) {
                return -1;
			}
        } else {
            phys_block = bitmap_alloc();
            if (phys_block == 0) return -1;
            if (extent_add(record_id, logical_block, phys_block, 1) != 0) {
                bitmap_clear(phys_block);
                return -1;
            }
            memset(block, 0, sizeof(block));
        }

		chunk = len - done;
        if (chunk > SMKFS_BLOCK_SIZE - block_offset) {
            chunk = SMKFS_BLOCK_SIZE - block_offset;
		}

        memcpy(block + block_offset, in + done, chunk);

        if (write_block(phys_block, block) != 0) {
            return -1;
		}

        done += chunk;

	}

	if (new_size != file_size) {
        record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_FSIZE, &new_size, sizeof(new_size));
        rec.attr_count++;
        if (record_write(record_id, &rec, attr_buf) != 0) {
            return -1;
		}
    }

    return (int)len;
}

int smkfs_truncate(uint64_t record_id, uint64_t new_size) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *fsize_ptr;
    uint64_t old_size;
    uint64_t old_blocks;
    uint64_t new_blocks;

    if (!mounted || record_id == 0) return -1;

    if (record_read(record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return -1;
	}

    if (rec.object_type != SMKFS_ROT_FILE) return -1;

    if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (void **)&fsize_ptr, NULL) != 0) {
        old_size = 0;
	} else {
        old_size = *fsize_ptr;
	}

    if (new_size == old_size) return 0;

    if (new_size < old_size) {
        old_blocks = (old_size + SMKFS_BLOCK_SIZE - 1) / SMKFS_BLOCK_SIZE;
        new_blocks = (new_size + SMKFS_BLOCK_SIZE - 1) / SMKFS_BLOCK_SIZE;

        if (new_blocks < old_blocks) {
            void *ext_data;
            size_t ext_len;
            if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &ext_data, &ext_len) == 0) {
                uint32_t num = ext_len / sizeof(smkfs_extent_t);
                smkfs_extent_t *ext = (smkfs_extent_t *)ext_data;
                for (uint32_t i = 0; i < num; i++) {
                    uint64_t ext_end = ext[i].logical_offset + ext[i].block_count;
                    if (ext[i].logical_offset >= new_blocks) {
                        bitmap_free_range(ext[i].physical_block, ext[i].block_count);
                        ext[i].block_count = 0;
                    } else if (ext_end > new_blocks) {
                        uint64_t keep = new_blocks - ext[i].logical_offset;
                        bitmap_free_range(ext[i].physical_block + keep, ext[i].block_count - keep);
                        ext[i].block_count = (uint32_t)keep;
                    }
                }
                record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_EXTENTS, ext, num * sizeof(smkfs_extent_t));
            }
        }
    }

    record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_FSIZE, &new_size, sizeof(new_size));
    rec.attr_count++;
    return record_write(record_id, &rec, attr_buf);
}

int smkfs_getattr(uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size) {
	if (!mounted || !rec || !attr_buf || record_id == 0) return -1;
	return record_read(record_id, rec, attr_buf, buf_size);
}

int smkfs_setattr(uint64_t record_id, uint16_t attr_type, const void *data, size_t len) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;

    if (!mounted || !data || record_id == 0) return -1;

    if (record_read(record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return -1;
	}

    if (record_add_attr(attr_buf, sizeof(attr_buf), attr_type, data, len) != 0) {
        return -1;
	}

    rec.attr_count++;
    return record_write(record_id, &rec, attr_buf);
}

/* ~~~ Level 2: User ~~~ */

int path_lookup(const char *path, uint64_t *out_record) {
    const char *p;
    char name[SMKFS_NAME_LEN];
    uint64_t current;
    int i;
    uint8_t path_drive;

    if (!path || path[1] != ':' || path[2] != '/') return -1;

    path_drive = path[0] - 'A';
    if (path_drive != drive_num) return -1;

    current = sb.root_record;
    p = path + 3;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        i = 0;
        while (*p && *p != '/' && i < SMKFS_NAME_LEN - 1) {
            name[i++] = *p++;
        }
        name[i] = '\0';

        if (smkfs_lookup_by_name(current, name, &current) != 0) {
            return -1;
        }
    }

    if (out_record) *out_record = current;
    return 0;
}

int smkfs_open(const char *path, int flags) {
    uint64_t record_id;
    int fd;

    if (!mounted || !path || path[1] != ':' || path[2] != '/') return -1;

    if (path_lookup(path, &record_id) != 0) {
        if (flags & SMKFS_O_CREATE) {
            if (smkfs_create_file(path, SMKFS_PERM_WRITE | SMKFS_PERM_WRITE) != 0) return -1;
            if (path_lookup(path, &record_id) != 0) return -1;
        } else {
            return -1;
        }
    }

    for (fd = 0; fd < SMKFS_FD_MAX; fd++) {
        if (!fd_table[fd].used) break;
    }
    if (fd >= SMKFS_FD_MAX) return -1;

    fd_table[fd].used = 1;
    fd_table[fd].record_id = record_id;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = flags;

    if (flags & SMKFS_O_APPEND) {
        uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
        smkfs_record_t rec;
        uint64_t *fsize_ptr;
        if (record_read(record_id, &rec, attr_buf, sizeof(attr_buf)) == 0) {
            if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (void **)&fsize_ptr, NULL) == 0) {
                fd_table[fd].offset = *fsize_ptr;
            }
        }
    }

    return fd;
}

int smkfs_close(int fd){
	if (fd < 0 || fd >= SMKFS_FD_MAX) return -1;
	if (!fd_table[fd].used) return -1;

	fd_table[fd].used = 0;
    fd_table[fd].record_id = 0;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = 0;

	return 0;
}

int smkfs_read_file(int fd, void *buf, size_t len) {
	int ret;

	if (fd < 0 || fd >= SMKFS_FD_MAX) return -1;
	if (!fd_table[fd].used) return -1;
	if (!buf) return -1;

	ret = smkfs_read(fd_table[fd].record_id, fd_table[fd].offset, len, buf);
    if (ret > 0) fd_table[fd].offset += ret;

	return ret;
}

int smkfs_write_file(int fd, const void *buf, size_t len) {
    int ret;

    if (fd < 0 || fd >= SMKFS_FD_MAX) return -1;
    if (!fd_table[fd].used) return -1;
    if (!buf) return -1;

    ret = smkfs_write(fd_table[fd].record_id, fd_table[fd].offset, len, buf);
    if (ret > 0) fd_table[fd].offset += ret;

    return ret;
}

int smkfs_seek(int fd, int64_t offset, int whence) {
	uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *fsize_ptr;
    uint64_t file_size = 0;
    int64_t new_offset;

	if (fd < 0 || fd >= SMKFS_FD_MAX) return -1;
	if (!fd_table[fd].used) return -1;

	switch (whence) {
		case SMKFS_SEEK_END: {
			if (record_read(fd_table[fd].record_id, &rec, attr_buf, sizeof(attr_buf)) == 0) {
				if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (void **)&fsize_ptr, NULL) == 0) {
					file_size = *fsize_ptr;
				}
			}

			new_offset = (int64_t)file_size + offset;

			break;
		}
		case SMKFS_SEEK_CUR: {
			new_offset = (int64_t)fd_table[fd].offset + offset;
		}
		case SMKFS_SEEK_SET: {
			new_offset = offset;
		}
		default: {
			return -1;
		}
	}

	if (new_offset < 0) return -1;

	fd_table[fd].offset = (uint64_t)new_offset;
	return (int)fd_table[fd].offset;
}

int smkfs_create_file(const char *path, uint16_t permissions) {
    uint64_t parent;
    char name[SMKFS_NAME_LEN];
    const char *last_slash;
    const char *name_start;
    uint64_t new_record;

    if (!mounted || !path || path[1] != ':' || path[2] != '/') return -1;

    last_slash = strrchr(path, '/');
    if (!last_slash || last_slash == path + 2) {
        parent = sb.root_record;
        name_start = path + 3;
    } else {
        char parent_path[SMKFS_NAME_LEN];
        int len = last_slash - path;
        if (len >= SMKFS_NAME_LEN) return -1;
        memcpy(parent_path, path, len);
        parent_path[len] = '\0';
        if (path_lookup(parent_path, &parent) != 0) return -1;
        name_start = last_slash + 1;
    }

    int i = 0;
    while (*name_start && *name_start != '/' && i < SMKFS_NAME_LEN - 1) {
        name[i++] = *name_start++;
    }
    name[i] = '\0';

    if (smkfs_create_record(SMKFS_ROT_FILE, parent, name, &new_record) != 0) {
        return -1;
	}

    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    if (record_read(new_record, &rec, attr_buf, sizeof(attr_buf)) == 0) {
        record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_PERMISSIONS, &permissions, sizeof(permissions));
        rec.attr_count++;
        record_write(new_record, &rec, attr_buf);
    }

    return 0;
}

int smkfs_delete_file(const char *path) {
	uint64_t record_id;

	if (!mounted || !path || path[1] != ':' || path[2] != '/') return -1;

    if (path_lookup(path, &record_id) != 0) return -1;

    return smkfs_delete_record(record_id);
}

int smkfs_mkdir(const char *path) {
    uint64_t parent;
    char name[SMKFS_NAME_LEN];
    const char *last_slash;
    const char *name_start;
    uint64_t new_record;

    if (!mounted || !path || path[1] != ':' || path[2] != '/') return -1;

    last_slash = strrchr(path, '/');
    if (!last_slash || last_slash == path + 2) {
        parent = sb.root_record;
        name_start = path + 3;
    } else {
        char parent_path[SMKFS_NAME_LEN];
        int len = last_slash - path;
        if (len >= SMKFS_NAME_LEN) return -1;
        memcpy(parent_path, path, len);
        parent_path[len] = '\0';
        if (path_lookup(parent_path, &parent) != 0) return -1;
        name_start = last_slash + 1;
    }

    int i = 0;
    while (*name_start && *name_start != '/' && i < SMKFS_NAME_LEN - 1) {
        name[i++] = *name_start++;
    }
    name[i] = '\0';

    return smkfs_create_record(SMKFS_ROT_DIR, parent, name, &new_record);
}

int smkfs_rmdir(const char *path) {
    uint64_t record_id;

    if (!mounted || !path || path[1] != ':' || path[2] != '/') return -1;

    if (path_lookup(path, &record_id) != 0) return -1;

    return smkfs_delete_record(record_id);
}

int readdir_cb(const char *key, uint64_t value, void *ctx) {
    readdir_ctx_t *c = (readdir_ctx_t *)ctx;
    if (c->count >= c->max) return -1;
    strncpy(c->entries[c->count].name, key, SMKFS_NAME_LEN - 1);
    c->entries[c->count].name[SMKFS_NAME_LEN - 1] = '\0';
    c->entries[c->count].record_id = value;
    c->count++;
    return 0;
}

int smkfs_readdir(const char *path, smkfs_dirent_t *entries, size_t max_entries, size_t *out_count) {
    uint64_t dir_record;
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *btree_root;
    readdir_ctx_t ctx;

    if (!mounted || !path || !entries || !out_count) return -1;
    if (path[1] != ':' || path[2] != '/') return -1;

    if (path_lookup(path, &dir_record) != 0) return -1;

    if (record_read(dir_record, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return -1;
	}

    if (rec.object_type != SMKFS_ROT_DIR) return -1;

    if (record_find_attr(attr_buf, SMKFS_ATTRT_DATA, (void **)&btree_root, NULL) != 0) {
        return -1;
	}

    ctx.entries = entries;
    ctx.max = max_entries;
    ctx.count = 0;

    btree_iterate(*btree_root, readdir_cb, &ctx);

    *out_count = ctx.count;
    return 0;
}

int smkfs_stat(const char *path, smkfs_record_t *rec, void *attr_buf, size_t buf_size) {
    uint64_t record_id;

    if (!mounted || !path || !rec || !attr_buf || path[1] != ':' || path[2] != '/') {
        return -1;
	}

    if (path_lookup(path, &record_id) != 0) return -1;

    return smkfs_getattr(record_id, rec, attr_buf, buf_size);
}

int smkfs_chmod(const char *path, uint16_t permissions) {
    uint64_t record_id;

    if (!mounted || !path || path[1] != ':' || path[2] != '/') return -1;

    if (path_lookup(path, &record_id) != 0) return -1;

    return smkfs_setattr(record_id, SMKFS_ATTRT_PERMISSIONS, &permissions, sizeof(permissions));
}

int smkfs_chown(const char *path, uint32_t uid, uint32_t gid) {
    uint64_t record_id;
    uint64_t owner = ((uint64_t)uid << 32) | gid;

    if (!mounted || !path || path[1] != ':' || path[2] != '/') return -1;

    if (path_lookup(path, &record_id) != 0) return -1;

    return smkfs_setattr(record_id, SMKFS_ATTRT_OWNER, &owner, sizeof(owner));
}

/* ~~~ Level 1: Admin ~~~ */

int smkfs_mkfs(uint8_t drive, uint64_t total_blocks);
int smkfs_fsck(uint8_t drive);
int smkfs_dump_superblock(void);
int smkfs_dump_record(uint64_t record_id);
int smkfs_dump_journal(void);
int smkfs_dump_btree(uint64_t root_block);