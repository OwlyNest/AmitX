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
static smkfs_superblock_t SMKFS_UNUSED sb;
static int SMKFS_UNUSED mounted = 0;
static uint8_t SMKFS_UNUSED drive_num = 0;
static uint64_t SMKFS_UNUSED journal_next_sequence = 1;
static int SMKFS_UNUSED journal_in_transaction = 0;
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

/* --- Functions ---*/

/* ~~~ Level 4: Internal ~~~ */

/* Block I/O */

static int SMKFS_UNUSED read_block(uint64_t block, void *buf) {
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

static int SMKFS_UNUSED write_block(uint64_t block, const void *buf) {
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

static void SMKFS_UNUSED header_init(smkfs_header_t *h, uint16_t type, uint32_t length, uint32_t flags) {
	memcpy(h->magic, SMKFS_MAGIC, 4);
	h->version = SMKFS_VERSION;
	h->type = type;
	h->length = length;
	h->flags = flags;
	h->checksum = 0;
}

static int SMKFS_UNUSED header_validate(const smkfs_header_t *h, uint16_t expected_type) {
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

static uint32_t SMKFS_UNUSED checksum_compute(const void *data, size_t len) {
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

static void SMKFS_UNUSED header_checksum_update(smkfs_header_t *h, const void *data, size_t len) {
	h->checksum = 0; // compute assumes checksum == 0;
	h->checksum = checksum_compute(data, len);
}

static int SMKFS_UNUSED header_checksum_verify(const smkfs_header_t *h, const void *data, size_t len) {
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

static void SMKFS_UNUSED bitmap_set(uint64_t block) {
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

static void SMKFS_UNUSED bitmap_clear(uint64_t block) {
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

static int SMKFS_UNUSED bitmap_test(uint64_t block) {
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

/*
 * TODO:
 * optimize
*/
static uint64_t SMKFS_UNUSED bitmap_alloc_range(uint32_t count) {
	if (count == 0) return 0;

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
	return 0;
}

static uint64_t SMKFS_UNUSED bitmap_alloc(void) {
	return bitmap_alloc_range(1);
}

static void SMKFS_UNUSED bitmap_free_range(uint64_t start, uint32_t count) {
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

static int SMKFS_UNUSED record_read(uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size) {
	uint8_t block[SMKFS_BLOCK_SIZE];
	if (read_block(record_id, block) != 0) return -1;

	memcpy(rec, block, sizeof(smkfs_record_t));

	if (header_validate(&rec->header, SMKFS_ST_RECORD) != 0) return -1;
	if (header_checksum_verify(&rec->header, block, rec->header.length) != 0) return -1;

	size_t attr_len = rec->header.length - sizeof(smkfs_record_t);
	if (attr_len > buf_size) attr_len = buf_size;

	memcpy(attr_buf, block + sizeof(smkfs_record_t), attr_len);
	return (int)attr_len;
}

static int SMKFS_UNUSED record_write(uint64_t record_id, const smkfs_record_t *rec, const void *attr_buf) {
	uint8_t block[SMKFS_BLOCK_SIZE];
	size_t total_len = rec->header.length;

	if (total_len > SMKFS_BLOCK_SIZE) return -1;

	memcpy(block, rec, sizeof(smkfs_record_t));
	memcpy(block + sizeof(smkfs_record_t), attr_buf, total_len - sizeof(smkfs_record_t));

	header_checksum_update(&((smkfs_record_t *)block)->header, block, total_len);

	return write_block(record_id, block);
}

static uint64_t SMKFS_UNUSED record_alloc(uint16_t object_type) {
	uint64_t block = bitmap_alloc();
	if (block == 0) return 0;

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

static void SMKFS_UNUSED record_free(uint64_t record_id) {
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

static int SMKFS_UNUSED record_find_attr(const void *attr_buf, uint16_t attr_type, void **out_attr, size_t *out_len) {
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

static int SMKFS_UNUSED record_add_attr(void *attr_buf, size_t buf_size, uint16_t attr_type, const void *data, size_t data_len) {
	uint8_t *ptr = (uint8_t *)attr_buf;
	size_t used = 0;

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

static int SMKFS_UNUSED record_remove_attr(void *attr_buf, uint16_t attr_type) {
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

static void SMKFS_UNUSED extent_remove_all(uint64_t record_id) {
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
	if (!node || !payload || block == 0) return -1;
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
	if (!node || !payload || block == 0) return -1;
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
		if (new_leaf == 0) return -1;
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


static int SMKFS_UNUSED btree_search(uint64_t root_block, const char *key, uint64_t *out_value) {
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

static int SMKFS_UNUSED btree_insert(uint64_t root_block, const char *key, uint64_t value, uint64_t *new_root) {
	uint64_t new_block = 0;
	smkfs_btree_node_t node;
	smkfs_btree_leaf_entry_t entries[64];

	if (!key || !new_root) return -1;
	if (root_block == 0) {
		new_block = bitmap_alloc();
		if (new_block == 0) return -1;
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

static int SMKFS_UNUSED btree_delete(uint64_t root_block, const char *key, uint64_t *new_root) {
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

static int SMKFS_UNUSED btree_iterate(uint64_t root_block, int (*cb)(const char *key, uint64_t value, void *ctx), void *ctx) {
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
	return 0;
}

static int SMKFS_UNUSED journal_log_write(uint64_t block, const void *old_data, const void *new_data, size_t len) {
	(void)block; (void)old_data; (void)new_data; (void)len;
	return 0;
}

static int SMKFS_UNUSED journal_log_alloc(uint64_t block, uint32_t count) {
	(void)block; (void)count;
	return 0;
}

static int SMKFS_UNUSED journal_log_free(uint64_t block, uint32_t count) {
	(void)block; (void)count;
	return 0;
}

static int SMKFS_UNUSED journal_commit(void) {
	return 0;
}

