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
SMKFS_STATUS disk_read_block(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PVOID buf);
SMKFS_STATUS read_block(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PVOID buf);
SMKFS_STATUS disk_write_block(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PCVOID  buf);
SMKFS_STATUS write_block(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PCVOID  buf);

/* --- Block Cache --- */
VOID block_cache_init(_SMKFS_MOUNT *mnt);
SMKFS_STATUS block_cache_read(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, _SMKFS_BLOCK_BUF **out_buf);
SMKFS_STATUS block_cache_write(_SMKFS_MOUNT *mnt, _SMKFS_BLOCK_BUF *buf);
SMKFS_STATUS block_cache_flush(_SMKFS_MOUNT *mnt);
VOID block_cache_shutdown(_SMKFS_MOUNT *mnt);

/* --- Header --- */
VOID header_init(_SMKFS_HEADER *h, SMKFS_STRUCT_TYPE type, ULONG length, ULONG flags);
SMKFS_STATUS header_validate(const _SMKFS_HEADER *h, SMKFS_STRUCT_TYPE expected_type);

/* --- Checksum --- */
ULONG checksum_compute(PCVOID  data, SIZE_T len);
VOID header_checksum_update(_SMKFS_HEADER *h, PCVOID  data, SIZE_T len);
SMKFS_STATUS header_checksum_verify(const _SMKFS_HEADER *h, PCVOID  data, SIZE_T len);
VOID crc32c_test_vectors(VOID);

/* --- Master Record Table --- */
SMKFS_STATUS mrt_format(_SMKFS_MOUNT *mnt, SMKFS_BLOCK start_block, ULONGLONG length);
SMKFS_STATUS mrt_init(_SMKFS_MOUNT *mnt, SMKFS_BLOCK start_block, ULONGLONG length);
SMKFS_STATUS mrt_alloc_entry(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID *out_record_id, SMKFS_GENERATION *out_generation);
SMKFS_STATUS mrt_update_entry(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, SMKFS_BLOCK new_physical_block, SMKFS_MRT_FLAGS flags);
SMKFS_STATUS mrt_free_entry(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id);
SMKFS_STATUS mrt_resolve(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, SMKFS_BLOCK *out_physical_block, SMKFS_MRT_FLAGS *out_flags, SMKFS_GENERATION *out_generation);

/* --- Bitmap --- */
VOID bitmap_set(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block);
VOID bitmap_clear(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block);
LONG bitmap_test(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block);
SMKFS_BLOCK bitmap_alloc_range(_SMKFS_MOUNT *mnt, ULONG count);
SMKFS_BLOCK bitmap_alloc(_SMKFS_MOUNT *mnt);
VOID bitmap_free_range(_SMKFS_MOUNT *mnt, SMKFS_BLOCK start, ULONG count);
SMKFS_STATUS bitmap_init_regions(_SMKFS_MOUNT *mnt);
VOID bitmap_shutdown(_SMKFS_MOUNT *mnt);

/* --- Record --- */
LONG record_read(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, _SMKFS_RECORD *rec, PVOID attr_buf, SIZE_T buf_size);
SMKFS_STATUS record_write(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, const _SMKFS_RECORD *rec, PCVOID  attr_buf);
SMKFS_RECORD_ID record_alloc(_SMKFS_MOUNT *mnt, SMKFS_OBJECT_TYPE object_type);
VOID record_free(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id);
SIZE_T attr_buf_total_len(PCVOID  attr_buf);

/* --- Attribute --- */
SMKFS_STATUS record_find_attr(PCVOID  attr_buf, SMKFS_ATTR_TYPE attr_type, PVOID *out_attr, SIZE_T *out_len);
SMKFS_STATUS record_add_attr(PVOID attr_buf, SIZE_T buf_size, SMKFS_ATTR_TYPE attr_type, PCVOID  data, SIZE_T data_len);
SMKFS_STATUS record_remove_attr(PVOID attr_buf, SMKFS_ATTR_TYPE attr_type);
SMKFS_STATUS record_remove_attr_id(PVOID attr_buf, SMKFS_ATTR_TYPE attr_type, SMKFS_ATTR_ID attr_id);
SMKFS_STATUS record_iterate_attr(PVOID attr_buf, SMKFS_ATTR_TYPE attr_type, LONG (*cb)(SMKFS_ATTR_ID attr_id, PVOID data, SIZE_T len, PVOID ctx), PVOID ctx);

/* --- Extent --- */
SMKFS_STATUS extent_resolve(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, SMKFS_LBLOCK logical_block, _SMKFS_EXTENT *out);
SMKFS_STATUS extent_add(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, SMKFS_LBLOCK logical_block, SMKFS_BLOCK physical_block, ULONG count);
void extent_remove_all(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id);

/* --- B+ Tree --- */
SMKFS_STATUS btree_search(_SMKFS_MOUNT *mnt, SMKFS_BLOCK root_block, PCCHAR key, ULONGLONG *out_value);
SMKFS_STATUS btree_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK root_block, PCCHAR key, ULONGLONG value, SMKFS_BLOCK *new_root);
SMKFS_STATUS btree_delete(_SMKFS_MOUNT *mnt, SMKFS_BLOCK root_block, PCCHAR key, SMKFS_BLOCK *new_root);
SMKFS_STATUS btree_iterate(_SMKFS_MOUNT *mnt, SMKFS_BLOCK root_block, LONG (*cb)(PCCHAR key, ULONGLONG value, PVOID ctx), PVOID ctx);
SMKFS_STATUS btree_node_read(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, _SMKFS_BTREE_NODE *node, PVOID payload, SIZE_T payload_size);
SMKFS_STATUS btree_node_write(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block_id, const _SMKFS_BTREE_NODE *node, PCVOID  payload, SIZE_T payload_size);
VOID btree_dump_recursive(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, LONG depth);

/* --- Journal --- */
SMKFS_STATUS journal_start_transaction(_SMKFS_MOUNT *mnt);
SMKFS_STATUS journal_log_write(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PCVOID  old_data, PCVOID  new_data, SIZE_T len);
SMKFS_STATUS journal_log_alloc(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, ULONG count);
SMKFS_STATUS journal_log_free(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, ULONG count);
SMKFS_STATUS journal_commit(_SMKFS_MOUNT *mnt);
SMKFS_STATUS journal_replay(_SMKFS_MOUNT *mnt);
SMKFS_STATUS journal_abort(_SMKFS_MOUNT *mnt);
SMKFS_STATUS journal_log_mrt_update(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, const _SMKFS_MRT_ENTRY *entry);

#endif