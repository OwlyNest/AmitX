/*
	* fs/amfs.h - [Enter description]
	* Author:   amity
	* Date:     Thu Jun 11 18:12:38 2026
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
#ifndef AMFS_H
#define AMFS_H

#define AMFS_MAGIC             "AMFS"
#define AMFS_VERSION           2

#define AMFS_SECTOR_SIZE       512
#define AMFS_MAX_INODES        64
#define AMFS_INODE_SIZE        64
#define AMFS_INODES_PER_SECTOR (AMFS_SECTOR_SIZE / AMFS_INODE_SIZE)
#define AMFS_INODE_SECTORS     (AMFS_MAX_INODES / AMFS_INODES_PER_SECTOR)

#define AMFS_DIRECT_BLOCKS     10
#define AMFS_NAME_LEN          30
#define AMFS_DIRENT_SIZE       32
#define AMFS_DIRENTS_PER_BLOCK (AMFS_SECTOR_SIZE / AMFS_DIRENT_SIZE)

#define AMFS_TYPE_FREE         0
#define AMFS_TYPE_FILE         1
#define AMFS_TYPE_DIR          2

#define AMFS_INVALID_INODE UINT32_MAX

/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
/* Superblock at sector 0 */
typedef struct {
    char     magic[4];
    uint32_t version;
    uint32_t total_sectors;
    uint32_t inode_count;
    uint32_t data_block_count;
    uint32_t inode_start;
    uint32_t data_start;
    uint32_t block_size;
    uint32_t root_inode;
} amfs_superblock_t;

/* Directory entry — one per potential file */
typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t blocks[AMFS_DIRECT_BLOCKS];
    uint32_t indirect;
    uint32_t parent;
    uint32_t reserved[2];
} amfs_inode_t;

typedef struct {
    uint32_t inode;
    char     name[AMFS_NAME_LEN];
} amfs_dirent_t;


/* --- Globals ---*/

/* --- Prototypes ---*/
int amfs_mkfs(uint32_t total_sectors);
int amfs_mount(void);
int amfs_mkdir(const char *path);
int amfs_create(const char *path);
int amfs_write(const char *path, const char *data, uint32_t size);
int amfs_read(const char *path, char *buf, uint32_t buf_size);
int amfs_delete(const char *path);
void amfs_ls(const char *path);
int amfs_exists(const char *path);
int amfs_is_mounted(void);
#endif