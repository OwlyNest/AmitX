/*
	* include/fs/smkfs_internal.h - SmKFS Internal Shared Declarations (G1)
	* Author:   amity
	* Date:     Wed Jul 29 17:32:05 2026
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
#ifndef __FS_SMKFS_INTERNAL_H__
#define __FS_SMKFS_INTERNAL_H__
/* --- Includes ---*/
#include <fs/smkfs.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals --- */

/* --- Prototypes ---*/

/* --- Block I/O --- */
int read_block(smkfs_mount_t *mnt, uint64_t block, void *buf);
int write_block(smkfs_mount_t *mnt, uint64_t block, const void *buf);

/* --- Header --- */
void header_init(smkfs_header_t *h, uint16_t type, uint32_t length, uint32_t flags);
int header_validate(const smkfs_header_t *h, uint16_t expected_type);

/* --- Checksum --- */
uint32_t checksum_compute(const void *data, size_t len);
void header_checksum_update(smkfs_header_t *h, const void *data, size_t len);
int header_checksum_verify(const smkfs_header_t *h, const void *data, size_t len);
void crc32c_test_vectors(void);

/* --- Master Record Table --- */
int mrt_init(smkfs_mount_t *mnt, uint64_t start_block, uint64_t capacity);
int mrt_alloc_entry(smkfs_mount_t *mnt, uint64_t *out_record_id);
int mrt_update_entry(smkfs_mount_t *mnt, uint64_t record_id, uint64_t new_physical_block, uint16_t flags);
int mrt_free_entry(smkfs_mount_t *mnt, uint64_t record_id);
int mrt_resolve(smkfs_mount_t *mnt, uint64_t record_id, uint64_t *out_physical_block, uint16_t *out_flags, uint32_t *out_generation);

/* --- Bitmap --- */
void bitmap_set(smkfs_mount_t *mnt, uint64_t block);
void bitmap_clear(smkfs_mount_t *mnt, uint64_t block);
int bitmap_test(smkfs_mount_t *mnt, uint64_t block);
uint64_t bitmap_alloc_range(smkfs_mount_t *mnt, uint32_t count);
uint64_t bitmap_alloc(smkfs_mount_t *mnt);
void bitmap_free_range(smkfs_mount_t *mnt, uint64_t start, uint32_t count);

/* --- Record --- */
int record_read(smkfs_mount_t *mnt, uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size);
int record_write(smkfs_mount_t *mnt, uint64_t record_id, const smkfs_record_t *rec, const void *attr_buf);
uint64_t record_alloc(smkfs_mount_t *mnt, uint16_t object_type);
void record_free(smkfs_mount_t *mnt, uint64_t record_id);
size_t attr_buf_total_len(const void *attr_buf);

/* --- Attribute --- */
int record_find_attr(const void *attr_buf, uint16_t attr_type, void **out_attr, size_t *out_len);
int record_add_attr(void *attr_buf, size_t buf_size, uint16_t attr_type, const void *data, size_t data_len);
int record_remove_attr(void *attr_buf, uint16_t attr_type);

/* --- Extent --- */
int extent_resolve(smkfs_mount_t *mnt, uint64_t record_id, uint64_t logical_block, smkfs_extent_t *out);
int extent_add(smkfs_mount_t *mnt, uint64_t record_id, uint64_t logical_block, uint64_t physical_block, uint32_t count);
void extent_remove_all(smkfs_mount_t *mnt, uint64_t record_id);

/* --- B+ Tree --- */
int btree_search(smkfs_mount_t *mnt, uint64_t root_block, const char *key, uint64_t *out_value);
int btree_insert(smkfs_mount_t *mnt, uint64_t root_block, const char *key, uint64_t value, uint64_t *new_root);
int btree_delete(smkfs_mount_t *mnt, uint64_t root_block, const char *key, uint64_t *new_root);
int btree_iterate(smkfs_mount_t *mnt, uint64_t root_block, int (*cb)(const char *key, uint64_t value, void *ctx), void *ctx);
int btree_node_read(smkfs_mount_t *mnt, uint64_t block, smkfs_btree_node_t *node, void *payload, size_t payload_size);
int btree_node_write(smkfs_mount_t *mnt, uint64_t block_id, const smkfs_btree_node_t *node, const void *payload, size_t payload_size);
void btree_dump_recursive(smkfs_mount_t *mnt, uint64_t block, int depth);

/* --- Journal --- */
int journal_start_transaction(smkfs_mount_t *mnt);
int journal_log_write(smkfs_mount_t *mnt, uint64_t block, const void *old_data, const void *new_data, size_t len);
int journal_log_alloc(smkfs_mount_t *mnt, uint64_t block, uint32_t count);
int journal_log_free(smkfs_mount_t *mnt, uint64_t block, uint32_t count);
int journal_commit(smkfs_mount_t *mnt);
int journal_replay(smkfs_mount_t *mnt);

#endif