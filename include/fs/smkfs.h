/*
 * include/fs/smkfs.h - SmKFS Public API Header (G1)
 * Author:   amity
 * Date:     Thu Jul 23 11:40:08 2026
 * Copyright (C) 2026 OwlyNest
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

#ifndef __FS_SMKFS_H__
#define __FS_SMKFS_H__

/* --- Macros --- */

/* Magic And Version */
#define SMKFS_MAGIC             "SmKF"
#define SMKFS_VERSION           2
#define SMKFS_NAME_LEN          256

/* Block Size */
#define SMKFS_SECTOR_SIZE       512
#define SMKFS_BLOCK_SIZE        4096

/* Structure Types */
#define SMKFS_ST_SUPERBLOCK     0x0001
#define SMKFS_ST_RECORD         0x0002
#define SMKFS_ST_BTREE_NODE     0x0003
#define SMKFS_ST_JOURNAL_HDR    0x0004
#define SMKFS_ST_JOURNAL_ENT    0x0005
#define SMKFS_ST_BITMAP         0x0006
#define SMKFS_ST_ALLOC_META     0x0007
#define SMKFS_ST_MRT            0x0008

/* Record Object Types */
#define SMKFS_ROT_FILE          0x01
#define SMKFS_ROT_DIR           0x02
#define SMKFS_ROT_SYMLINK       0x03
#define SMKFS_ROT_DEVICE        0x04

/* Attribute Types */
#define SMKFS_ATTRT_END         0x0000
#define SMKFS_ATTRT_NAME        0x0001
#define SMKFS_ATTRT_DATA        0x0002
#define SMKFS_ATTRT_FSIZE       0x0003
#define SMKFS_ATTRT_PERMISSIONS 0x0004
#define SMKFS_ATTRT_OWNER       0x0005
#define SMKFS_ATTRT_TIMESTAMPS  0x0006
#define SMKFS_ATTRT_PARENT      0x0007
#define SMKFS_ATTRT_EXTENTS     0x0008
#define SMKFS_ATTRT_SYMLINK     0x0009
#define SMKFS_ATTRT_DEVICE      0x000A

/* Attribute Behavior Flags */
#define SMKFS_ATTRF_UNIQUE      0x0001
#define SMKFS_ATTRF_REQUIRED    0x0002
#define SMKFS_ATTRF_RESIDENT    0x0004

/* Flags */
#define SMKFS_FLA_RESIDENT      0x00000001
#define SMKFS_FLA_DELETED       0x00000002
#define SMKFS_FLA_COMPRESSED    0x00000004
#define SMKFS_FLA_ENCRYPTED     0x00000008

/* MRT entry Flags */
#define SMKFS_MRTF_ALLOCATED    0x0001
#define SMKFS_MRTF_OVERFLOW     0x0002
#define SMKFS_MRTF_DELETED      0x0004

/* B+ Tree Nodes */
#define SMKFS_BTR_MAX_KEY       255
#define SMKFS_BTN_LEAF          0x1
#define SMKFS_BTN_ROOT          0x2

/* Directory ENntry Flags */
#define SMKFS_DENTF_NORMAL      0x0000
#define SMKFS_DENTF_DOT         0x0001
#define SMKFS_DENTF_DOTDOT      0x0002

/* Journal OPerations */
#define SMKFS_JOP_WRITE         1
#define SMKFS_JOP_ALLOC         2
#define SMKFS_JOP_FREE          3
#define SMKFS_JOP_COMMIT        4
#define SMKFS_JOP_MRT_UPDATE    5
#define SMKFS_JOP_CHECKPOINT    6

/* Superblock Flags */
#define SMKFS_SBF_CLEAN         0x00000001
#define SMKFS_SBF_ERRORS        0x00000002

/* File Descriptor */
#define SMKFS_FD_MAX            16

/* Open flags */
#define SMKFS_O_RDONLY          0x0000
#define SMKFS_O_WRONLY          0x0001
#define SMKFS_O_RDWR            0x0002
#define SMKFS_O_CREATE          0x0004
#define SMKFS_O_TRUNC           0x0008
#define SMKFS_O_APPEND          0x0010

/* SEEK flags */
#define SMKFS_SEEK_SET          0
#define SMKFS_SEEK_CUR          1
#define SMKFS_SEEK_END          2

/* Permissions */
#define SMKFS_PERM_WRITE        0x0080
#define SMKFS_PERM_EXEC         0x0040
#define SMKFS_PERM_READ         0x0100

/* Error Codes */
#define SMKFS_OK                0
#define SMKFS_ERR_IO            -1
#define SMKFS_ERR_NOMEM         -2
#define SMKFS_ERR_NOTFOUND      -3
#define SMKFS_ERR_EXISTS        -4
#define SMKFS_ERR_NOSPC         -5
#define SMKFS_ERR_INVAL         -6
#define SMKFS_ERR_CORRUPT       -7
#define SMKFS_ERR_NOTEMPTY      -8
#define SMKFS_ERR_ROFS          -9
#define SMKFS_ERR_JOURNAL       -10
#define SMKFS_ERR_TOO_BIG       -11

/* --- Includes --- */
#include <stdint.h>
#include <stddef.h>
#include <internal/amitx_macros.h>

/* --- Typedefs - Structs - Enums --- */

/* Canonical header (32B) */
typedef struct {
    char     magic[4];
    uint16_t version;
    uint16_t type;
    uint32_t length;
    uint32_t flags;
    uint32_t checksum;
    uint32_t reserved[3];
} __attribute__((__packed__)) smkfs_header_t;
_Static_assert(sizeof(smkfs_header_t) == 32, "SmKFS Header size changed");

/* Master Record Table entry (16B) */
typedef struct {
    uint64_t physical_block;      /* 0 = unallocated / free slot */
    uint16_t flags;               /* SMKFS_MRTF_* */
    uint16_t reserved;
    uint32_t generation;          /* Incremented on slot reuse */
} __attribute__((__packed__)) smkfs_mrt_entry_t;
_Static_assert(sizeof(smkfs_mrt_entry_t) == 16, "SmKFS MRT entry size changed");

/* Extent (20B) */
typedef struct {
    uint64_t logical_offset;
    uint64_t physical_block;
    uint32_t block_count;
} __attribute__((__packed__)) smkfs_extent_t;
_Static_assert(sizeof(smkfs_extent_t) == 20, "SmKFS Extent size changed");

/* Attribute header (12B) */
typedef struct {
    uint16_t type;
    uint16_t flags;
    uint32_t id;
    uint32_t length;
} __attribute__((__packed__)) smkfs_attr_header_t;
_Static_assert(sizeof(smkfs_attr_header_t) == 12, "SmKFS Attribute Header size changed");

/* Attribute behavior definition (in-memory only) */
typedef struct {
    uint16_t     type;
    const char   *name;
    uint32_t     flags;
    size_t       fixed_size;
    int          (*validate)(const void *data, size_t len);
    void         (*debug_print)(const void *data, size_t len);
} smkfs_attr_def_t;

/* B+ tree node header (56B) */
typedef struct {
    smkfs_header_t header;
    uint64_t       parent_block;
    uint32_t       flags;
    uint32_t       key_count;
    uint64_t       right_sibling;
} __attribute__((__packed__)) smkfs_btree_node_t;
_Static_assert(sizeof(smkfs_btree_node_t) == 56, "B+ tree node size changed");

/* In-memory leaf entry (264B) */
typedef struct {
    uint64_t record_id;
    char     name[SMKFS_NAME_LEN];
} smkfs_btree_leaf_entry_t;

/* In-memory index entry (25B) */
typedef struct {
    uint64_t child_block;
    char     prefix[16];
    uint8_t  prefix_len;
} smkfs_btree_index_entry_t;


/* Record header v2 (56B) */
typedef struct {
    smkfs_header_t  header;
    uint64_t        record_id;        /* Logical ID (MRT index) */
    uint16_t        object_type;
    uint16_t        attr_count;
    uint32_t        link_count;       /* Directory entries pointing here */
    uint64_t        generation;       /* Matches MRT generation */
} __attribute__((__packed__)) smkfs_record_t;
_Static_assert(sizeof(smkfs_record_t) == 56, "SmKFS Record size changed");

/* Journal entry v2 (62B + payload) */
typedef struct {
    smkfs_header_t  header;
    uint64_t        sequence;
    uint64_t        target_block;
    uint32_t        operation;
    uint32_t        data_length;
    uint64_t        record_id;        /* Logical record for fsck */
} __attribute__((__packed__)) smkfs_journal_entry_t;
_Static_assert(sizeof(smkfs_journal_entry_t) == 64, "SmKFS Journal entry size changed");

/* Superblock v2 (312 bytes) */
typedef struct {
    smkfs_header_t  header;
    uint64_t        total_blocks;
    uint64_t        free_blocks;
    uint64_t        record_count;
    uint64_t        next_record_id;   /* Next free MRT slot */
    uint64_t        mrt_start;
    uint64_t        mrt_length;       /* Blocks reserved for MRT */
    uint64_t        mrt_capacity;     /* Max records (MRT entries) */
    uint64_t        mrt_free_count;
    uint64_t        root_record_id;   /* Logical ID of root dir */
    uint64_t        journal_start;
    uint64_t        journal_length;
    uint64_t        journal_head;       /* Next free journal slot */
    uint64_t        journal_tail;       /* Oldest uncheckpointed entry */
    uint64_t        journal_sequence;   /* Monotonic transaction counter */
    uint64_t        bitmap_start;
    uint64_t        bitmap_length;
    uint64_t        alloc_meta_start;
    uint64_t        alloc_meta_length;
    uint64_t        data_start;
    uint32_t        block_size;
    uint32_t        flags;            /* SMKFS_SBF_* */
    uint8_t         uuid[16];
    char            volume_name[64];
    uint64_t        creation_time;
    uint64_t        last_mount_time;
    uint32_t        mount_count;
    uint32_t        max_mount_count;
    uint32_t        reserved[4];
} __attribute__((__packed__)) smkfs_superblock_t;
_Static_assert(sizeof(smkfs_superblock_t) == 312, "SmKFS Superblock size changed");

/* On-disk directory entry (B+ tree value) (16B) */
typedef struct {
    uint64_t record_id;
    uint32_t name_hash;     /* Fast comparison filter */
    uint16_t flags;         /* SMKFS_DENTF_* */
    uint16_t name_len;      /* Actual name length */
} __attribute__((__packed__)) smkfs_dirent_disk_t;
_Static_assert(sizeof(smkfs_dirent_disk_t) == 16, "SmKFS Dirent Disk size changed");

/* Directory entry (in-memory, userspace-facing, 264B) */
typedef struct {
    uint64_t record_id;
    char     name[SMKFS_NAME_LEN];
} smkfs_dirent_t;

/* File Descriptor */
typedef struct {
    int      used;
    uint64_t record_id;
    uint64_t offset;
    int      flags;
} smkfs_fd_t;

/* Per-mount context (G1 multi-mount support) */
typedef struct {
    uint8_t             drive_num;
    int                 mounted;
    smkfs_superblock_t  sb;
    uint64_t            journal_next_sequence;
    int                 journal_in_transaction;
    uint64_t            journal_write_pos;
    smkfs_fd_t          fd_table[SMKFS_FD_MAX];
} smkfs_mount_t;

/* Read Directory Context */
typedef struct {
    smkfs_dirent_t *entries;
    size_t          max;
    size_t          count;
} readdir_ctx_t;

/* --- Prototypes --- */

/* ~~~ Master Attribute Table ~~~ */
const smkfs_attr_def_t *smkfs_attr_lookup(uint16_t type);
const char *smkfs_attr_name(uint16_t type);
void smkfs_attr_debug_print(uint16_t type, const void *data, size_t len);

/* ~~~ Level 3: Kernel ~~~ */
int smkfs_mount(uint8_t drive, smkfs_mount_t *mnt); /* Done */
int smkfs_unmount(smkfs_mount_t *mnt); /* Done */
int smkfs_sync(smkfs_mount_t *mnt); /* Done */
int smkfs_lookup_by_name(smkfs_mount_t *mnt, uint64_t dir_record, const char *name, uint64_t *out_record); /* Done */
int smkfs_create_record(smkfs_mount_t *mnt, uint16_t object_type, uint64_t parent_dir, const char *name, uint64_t *out_record); /* Done */
int smkfs_delete_record(smkfs_mount_t *mnt, uint64_t record_id); /* Done */
int smkfs_rename(smkfs_mount_t *mnt, uint64_t record_id, uint64_t new_parent, const char *new_name); /* Done */
int smkfs_read(smkfs_mount_t *mnt, uint64_t record_id, uint64_t offset, size_t len, void *buf); /* Done */
int smkfs_write(smkfs_mount_t *mnt, uint64_t record_id, uint64_t offset, size_t len, const void *buf); /* Done */
int smkfs_truncate(smkfs_mount_t *mnt, uint64_t record_id, uint64_t new_size); /* Done */
int smkfs_getattr(smkfs_mount_t *mnt, uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size); /* Done */
int smkfs_setattr(smkfs_mount_t *mnt, uint64_t record_id, uint16_t attr_type, const void *data, size_t len);     /* Done */

/* ~~~ Level 2: User ~~~ */
int path_lookup(smkfs_mount_t *mnt, const char *path, uint64_t *out_record); /* Done */
int smkfs_open(smkfs_mount_t *mnt, const char *path, int flags); /* Done */
int smkfs_close(smkfs_mount_t *mnt, int fd); /* Done */
int smkfs_read_file(smkfs_mount_t *mnt, int fd, void *buf, size_t len); /* Done */
int smkfs_write_file(smkfs_mount_t *mnt, int fd, const void *buf, size_t len); /* Done */
int smkfs_seek(smkfs_mount_t *mnt, int fd, int64_t offset, int whence); /* Done */
int smkfs_create_file(smkfs_mount_t *mnt, const char *path, uint16_t permissions); /* Done */
int smkfs_delete_file(smkfs_mount_t *mnt, const char *path); /* Done */
int smkfs_mkdir(smkfs_mount_t *mnt, const char *path); /* Done */
int smkfs_rmdir(smkfs_mount_t *mnt, const char *path); /* Done */
int smkfs_readdir(smkfs_mount_t *mnt, const char *path, smkfs_dirent_t *entries, size_t max_entries, size_t *out_count); /* Done */
int smkfs_stat(smkfs_mount_t *mnt, const char *path, smkfs_record_t *rec, void *attr_buf, size_t buf_size); /* Done */
int smkfs_chmod(smkfs_mount_t *mnt, const char *path, uint16_t permissions); /* Done */
int smkfs_chown(smkfs_mount_t *mnt, const char *path, uint32_t uid, uint32_t gid); /* Done */

/* ~~~ Level 1: Admin ~~~ */
int smkfs_mkfs(uint8_t drive, uint64_t total_blocks); /* Done */
int smkfs_fsck(uint8_t drive); /* Done */
int smkfs_dump_superblock(smkfs_mount_t *mnt); /* Done */
int smkfs_dump_record(smkfs_mount_t *mnt, uint64_t record_id); /* Done */
int smkfs_dump_journal(smkfs_mount_t *mnt); /* Done */
int smkfs_dump_btree(smkfs_mount_t *mnt, uint64_t root_block); /* Done */

#endif