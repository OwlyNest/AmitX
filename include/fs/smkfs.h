/*
	* include/fs/smkfs.h - [Enter description]
	* Author:   amity
	* Date:     Thu Jul 23 11:40:08 2026
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
#ifndef __FS_SMKFS_H__
#define __FS_SMKFS_H__

/* Magic And Version */
#define SMKFS_MAGIC 			"SmKF"
#define SMKFS_VERSION 			1
#define SMKFS_NAME_LEN          256

/* Block Size */
#define SMKFS_SECTOR_SIZE 		512
#define SMKFS_BLOCK_SIZE 		4096

/* Structure Types */
#define SMKFS_ST_SUPERBLOCK 	0x0001
#define SMKFS_ST_RECORD 		0x0002
#define SMKFS_ST_BTREE_NODE 	0x0003
#define SMKFS_ST_JOURNAL_HDR 	0x0004
#define SMKFS_ST_JOURNAL_ENT 	0x0005
#define SMKFS_ST_BITMAP 		0x0006
#define SMKFS_ST_ALLOC_META 	0x0007

/* Record Object Types */
#define SMKFS_ROT_FILE 			0x01
#define SMKFS_ROT_DIR 			0x02
#define SMKFS_ROT_SYMLINK 		0x03
#define SMKFS_ROT_DEVICE 		0x04

/* ATTRibute Types */
#define SMKFS_ATTRT_END 		0x0000
#define SMKFS_ATTRT_NAME 		0x0001
#define SMKFS_ATTRT_DATA 		0x0002
#define SMKFS_ATTRT_FSIZE 		0x0003 // logical file size
#define SMKFS_ATTRT_PERMISSIONS 0x0004
#define SMKFS_ATTRT_OWNER       0x0005
#define SMKFS_ATTRT_TIMESTAMPS  0x0006
#define SMKFS_ATTRT_PARENT      0x0007
#define SMKFS_ATTRT_EXTENTS     0x0008
#define SMKFS_ATTRT_SYMLINK     0x0009
#define SMKFS_ATTRT_DEVICE      0x000A

/* FLAgs */
#define SMKFS_FLA_RESIDENT 		0x00000001 // 0b0001
#define SMKFS_FLA_DELETED  		0x00000002 // 0b0010
#define SMKFS_FLA_COMPRESSED 	0x00000004 // 0b0100
#define SMKFS_FLA_ENCRYPTED 	0x00000008 // 0b1000

/* B+ Tree Nodes */
#define SMKFS_BTR_MAX_KEY 255
#define SMKFS_BTN_LEAF 0x1
#define SMKFS_BTN_ROOT 0x2

/* Journal OPerations */
#define SMKFS_JOP_WRITE  1
#define SMKFS_JOP_ALLOC  2
#define SMKFS_JOP_FREE   3
#define SMKFS_JOP_COMMIT 4

/* File Descriptor */
#define SMKFS_FD_MAX 16

/* Open flags */
#define SMKFS_O_RDONLY  0x0000
#define SMKFS_O_WRONLY  0x0001
#define SMKFS_O_RDWR    0x0002
#define SMKFS_O_CREATE  0x0004
#define SMKFS_O_TRUNC   0x0008
#define SMKFS_O_APPEND  0x0010

/* SEEK flags*/
#define SMKFS_SEEK_SET 0
#define SMKFS_SEEK_CUR 1
#define SMKFS_SEEK_END 2

/* PERMissions */
#define SMKFS_PERM_WRITE   0x0080
#define SMKFS_PERM_EXEC    0x0040
#define SMKFS_PERM_READ    0x0100

/* --- Includes ---*/
#include <stdint.h>
#include <stddef.h>

/* --- Typedefs - Structs - Enums ---*/

/* Canonical header */
typedef struct {
	char 	 magic[4];
	uint16_t version;
	uint16_t type;
	uint32_t length;
	uint32_t flags;
	uint32_t checksum;
	uint32_t reserved[3];
} smkfs_header_t;

/* Extent */
typedef struct {
	uint64_t logical_offset;
	uint64_t physical_block;
	uint32_t block_count;
} smkfs_extent_t;

/* Attribute header */
typedef struct {
    uint16_t type;
    uint16_t flags;
    uint32_t id;
    uint32_t length;
} smkfs_attr_header_t;

/* B+ tree node header — unchanged */
typedef struct {
    smkfs_header_t header;
    uint64_t       parent_block;
    uint32_t       flags;
    uint32_t       key_count;
    uint64_t       right_sibling;
} smkfs_btree_node_t;

/* In-memory leaf entry */
typedef struct {
    uint64_t record_id;
    char     name[SMKFS_NAME_LEN];
} smkfs_btree_leaf_entry_t;

/* In-memory index entry */
typedef struct {
    uint64_t child_block;
    char     prefix[16];
    uint8_t  prefix_len;
} smkfs_btree_index_entry_t;

/* Record header */
typedef struct {
	smkfs_header_t	header;
	uint64_t 		record_id;
	uint16_t 		object_type;
	uint16_t 		attr_count;
} smkfs_record_t;

/* Journal entry */
typedef struct {
	smkfs_header_t 	header;
	uint64_t   		sequence;
	uint64_t 		target_block;
	uint32_t 		operation;
	uint32_t 		data_length;
} smkfs_journal_entry_t;

/* Superblock */
typedef struct {
	smkfs_header_t 	header;
	uint64_t 		total_blocks;
	uint64_t 		free_blocks;
	uint64_t 		record_count;
	uint64_t 		next_record_id;
	uint64_t 		root_record;
	uint64_t 		journal_start;
	uint64_t 		journal_length;
	uint64_t 		bitmap_start;
	uint64_t 		bitmap_length;
	uint64_t 		alloc_meta_start;
	uint64_t 		alloc_meta_length;
	uint64_t 		data_start;
	uint32_t 		block_size;
	uint32_t 		flags;
	// uuid
	// volume name
	// creation time
	// last mount time
} smkfs_superblock_t;

/* Directory entry (in memory) */
typedef struct {
	uint64_t record_id;
	char     name[SMKFS_NAME_LEN];
} smkfs_dirent_t;

/* File Descriptor */
typedef struct {
	int used;
	uint64_t record_id;
	uint64_t offset;
	int flags;
} smkfs_fd_t;

/* Read Directory Context */
typedef struct {
    smkfs_dirent_t *entries;
    size_t max;
    size_t count;
} readdir_ctx_t;

/* --- Globals ---*/

/* --- Prototypes ---*/

/* ~~~ Level 3: Kernel ~~~ */
int smkfs_mount(uint8_t drive);
int smkfs_unmount(void);
int smkfs_sync(void);
int smkfs_lookup_by_name(uint64_t dir_record, const char *name, uint64_t *out_record);
int smkfs_create_record(uint16_t object_type, uint64_t parent_dir, const char *name, uint64_t *out_record);
int smkfs_delete_record(uint64_t record_id);
int smkfs_rename(uint64_t record_id, uint64_t new_parent, const char *new_name);
int smkfs_read(uint64_t record_id, uint64_t offset, size_t len, void *buf);
int smkfs_write(uint64_t record_id, uint64_t offset, size_t len, const void *buf);
int smkfs_truncate(uint64_t record_id, uint64_t new_size);
int smkfs_getattr(uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size);
int smkfs_setattr(uint64_t record_id, uint16_t attr_type, const void *data, size_t len);

/* ~~~ Level 2: User ~~~ */
int path_lookup(const char *path, uint64_t *out_record);
int smkfs_open(const char *path, int flags);
int smkfs_close(int fd);
int smkfs_read_file(int fd, void *buf, size_t len);
int smkfs_write_file(int fd, const void *buf, size_t len);
int smkfs_seek(int fd, int64_t offset, int whence);
int smkfs_create_file(const char *path, uint16_t permissions);
int smkfs_delete_file(const char *path);
int smkfs_mkdir(const char *path);
int smkfs_rmdir(const char *path);
int smkfs_readdir(const char *path, smkfs_dirent_t *entries, size_t max_entries, size_t *out_count);
int smkfs_stat(const char *path, smkfs_record_t *rec, void *attr_buf, size_t buf_size);
int smkfs_chmod(const char *path, uint16_t permissions);
int smkfs_chown(const char *path, uint32_t uid, uint32_t gid);

/* ~~~ Level 1: Admin ~~~ */
int smkfs_mkfs(uint8_t drive, uint64_t total_blocks);
int smkfs_fsck(uint8_t drive);
int smkfs_dump_superblock(void);
int smkfs_dump_record(uint64_t record_id);
int smkfs_dump_journal(void);
int smkfs_dump_btree(uint64_t root_block);

#endif