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
SMKFS_STATUS read_block(smkfs_mount_t *mnt, SMKFS_BLOCK block, PVOID buf);
SMKFS_STATUS write_block(smkfs_mount_t *mnt, SMKFS_BLOCK block, PCVOID  buf);

/* --- Header --- */
VOID header_init(smkfs_header_t *h, SMKFS_STRUCT_TYPE type, ULONG length, ULONG flags);
SMKFS_STATUS header_validate(const smkfs_header_t *h, SMKFS_STRUCT_TYPE expected_type);

/* --- Checksum --- */
ULONG checksum_compute(PCVOID  data, SIZE_T len);
VOID header_checksum_update(smkfs_header_t *h, PCVOID  data, SIZE_T len);
SMKFS_STATUS header_checksum_verify(const smkfs_header_t *h, PCVOID  data, SIZE_T len);
VOID crc32c_test_vectors(VOID);

/* --- Master Record Table --- */
SMKFS_STATUS mrt_format(smkfs_mount_t *mnt, SMKFS_BLOCK start_block, ULONGLONG length);
SMKFS_STATUS mrt_init(smkfs_mount_t *mnt, SMKFS_BLOCK start_block, ULONGLONG length);
SMKFS_STATUS mrt_alloc_entry(smkfs_mount_t *mnt, SMKFS_RECORD_ID *out_record_id, SMKFS_GENERATION *out_generation);
SMKFS_STATUS mrt_update_entry(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id, SMKFS_BLOCK new_physical_block, SMKFS_MRT_FLAGS flags);
SMKFS_STATUS mrt_free_entry(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id);
SMKFS_STATUS mrt_resolve(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id, SMKFS_BLOCK *out_physical_block, SMKFS_MRT_FLAGS *out_flags, SMKFS_GENERATION *out_generation);

/* --- Bitmap --- */
VOID bitmap_set(smkfs_mount_t *mnt, SMKFS_BLOCK block);
VOID bitmap_clear(smkfs_mount_t *mnt, SMKFS_BLOCK block);
LONG bitmap_test(smkfs_mount_t *mnt, SMKFS_BLOCK block);
SMKFS_BLOCK bitmap_alloc_range(smkfs_mount_t *mnt, ULONG count);
SMKFS_BLOCK bitmap_alloc(smkfs_mount_t *mnt);
VOID bitmap_free_range(smkfs_mount_t *mnt, SMKFS_BLOCK start, ULONG count);

/* --- Record --- */
LONG record_read(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id, smkfs_record_t *rec, PVOID attr_buf, SIZE_T buf_size);
SMKFS_STATUS record_write(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id, const smkfs_record_t *rec, PCVOID  attr_buf);
SMKFS_RECORD_ID record_alloc(smkfs_mount_t *mnt, SMKFS_OBJECT_TYPE object_type);
VOID record_free(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id);
SIZE_T attr_buf_total_len(PCVOID  attr_buf);

/* --- Attribute --- */
SMKFS_STATUS record_find_attr(PCVOID  attr_buf, SMKFS_ATTR_TYPE attr_type, PVOID *out_attr, SIZE_T *out_len);
SMKFS_STATUS record_add_attr(PVOID attr_buf, SIZE_T buf_size, SMKFS_ATTR_TYPE attr_type, PCVOID  data, SIZE_T data_len);
SMKFS_STATUS record_remove_attr(PVOID attr_buf, SMKFS_ATTR_TYPE attr_type);

/* --- Extent --- */
SMKFS_STATUS extent_resolve(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id, SMKFS_LBLOCK logical_block, smkfs_extent_t *out);
SMKFS_STATUS extent_add(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id, SMKFS_LBLOCK logical_block, SMKFS_BLOCK physical_block, ULONG count);
void extent_remove_all(smkfs_mount_t *mnt, SMKFS_RECORD_ID record_id);

/* --- B+ Tree --- */
SMKFS_STATUS btree_search(smkfs_mount_t *mnt, SMKFS_BLOCK root_block, PCCHAR key, ULONGLONG *out_value);
SMKFS_STATUS btree_insert(smkfs_mount_t *mnt, SMKFS_BLOCK root_block, PCCHAR key, ULONGLONG value, SMKFS_BLOCK *new_root);
SMKFS_STATUS btree_delete(smkfs_mount_t *mnt, SMKFS_BLOCK root_block, PCCHAR key, SMKFS_BLOCK *new_root);
SMKFS_STATUS btree_iterate(smkfs_mount_t *mnt, SMKFS_BLOCK root_block, LONG (*cb)(PCCHAR key, ULONGLONG value, PVOID ctx), PVOID ctx);
SMKFS_STATUS btree_node_read(smkfs_mount_t *mnt, SMKFS_BLOCK block, smkfs_btree_node_t *node, PVOID payload, SIZE_T payload_size);
SMKFS_STATUS btree_node_write(smkfs_mount_t *mnt, SMKFS_BLOCK block_id, const smkfs_btree_node_t *node, PCVOID  payload, SIZE_T payload_size);
VOID btree_dump_recursive(smkfs_mount_t *mnt, SMKFS_BLOCK block, LONG depth);

/* --- Journal --- */
SMKFS_STATUS journal_start_transaction(smkfs_mount_t *mnt);
SMKFS_STATUS journal_log_write(smkfs_mount_t *mnt, SMKFS_BLOCK block, PCVOID  old_data, PCVOID  new_data, SIZE_T len);
SMKFS_STATUS journal_log_alloc(smkfs_mount_t *mnt, SMKFS_BLOCK block, ULONG count);
SMKFS_STATUS journal_log_free(smkfs_mount_t *mnt, SMKFS_BLOCK block, ULONG count);
SMKFS_STATUS journal_commit(smkfs_mount_t *mnt);
SMKFS_STATUS journal_replay(smkfs_mount_t *mnt);

#endif