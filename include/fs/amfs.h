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

#define AMFS_MAGIC          "AMFS"
#define AMFS_VERSION      1
#define AMFS_SECTOR_SIZE  512
#define AMFS_MAX_FILES    64
#define AMFS_NAME_LEN     32


/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
/* Superblock at sector 0 */
typedef struct __attribute__((packed)) {
    char     magic[4];
    uint32_t version;
    uint32_t total_sectors;
    uint32_t dir_sector;        /* Sector where directory starts */
    uint32_t data_sector;       /* Sector where data starts */
    uint32_t max_files;
    uint32_t sector_size;
    uint32_t file_count;        /* Currently used entries */
    uint8_t  reserved[484];
} amfs_superblock_t;

/* Directory entry — one per potential file */
typedef struct __attribute__((packed)) {
    char     name[AMFS_NAME_LEN];
    uint32_t start_sector;
    uint32_t size;
    uint8_t  used;              /* 0 = free, 1 = used */
    uint8_t  reserved[475];
} amfs_dirent_t;
/* --- Globals ---*/

/* --- Prototypes ---*/
int  amfs_mkfs(uint32_t total_sectors);
int  amfs_mount(void);
int  amfs_write_file(const char* name, const char* data, uint32_t size);
int  amfs_read_file(const char* name, char* buf, uint32_t buf_size);
int  amfs_delete_file(const char* name);
void amfs_ls(void);
int  amfs_exists(const char* name);
#endif