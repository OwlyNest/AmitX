/*
	* fs/amfs.c - [Enter description]
	* Author:   amity
	* Date:     Thu Jun 11 18:12:44 2026
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
#include <fs/amfs.h>
#include <hw/ide.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static amfs_superblock_t sb;
static int mounted = 0;


/* --- Prototypes ---*/
static int read_sector(uint32_t lba, void *buf);
static int write_sector(uint32_t lba, const void *buf);
//static void bitmap_set(uint32_t block);
static void bitmap_clear(uint32_t block);
//static int bitmap_test(uint32_t block);
static uint32_t bitmap_alloc(void);
static void inode_read(uint32_t inum, amfs_inode_t *inode);
static void inode_write(uint32_t inum, const amfs_inode_t *inode);
static uint32_t inode_alloc(uint32_t type);
static void inode_free(uint32_t inum);
static uint32_t block_for_offset(uint32_t inum, size_t offset, int alloc);
static int dir_find_entry(uint32_t dir_inum, const char *name, amfs_dirent_t *out, uint32_t *index);
static int dir_add_entry(uint32_t dir_inum, const char *name, uint32_t inum);
static int dir_remove_entry(uint32_t dir_inum, const char *name);
static int path_lookup(const char *path, uint32_t *out_inum);
static int path_lookup_parent(const char *path, uint32_t *parent, char *name);
/* --- Functions ---*/

static int read_sector(uint32_t lba, void *buf) {
	return ide_read_sectors(0, lba, 1, (uint16_t *)buf);
}

static int write_sector(uint32_t lba, const void *buf) {
	return ide_write_sectors(0, lba, 1, (const uint16_t *)buf);
}

// static void bitmap_set(uint32_t block) {
//     if (block >= sb.data_block_count) return;

//     uint8_t bitmap[AMFS_SECTOR_SIZE];
//     read_sector(1, bitmap);
//     bitmap[block / 8] |= (1 << (block % 8));
//     write_sector(1, bitmap);
// }

static void bitmap_clear(uint32_t block) {
    if (block >= sb.data_block_count) return;
    uint8_t bitmap[AMFS_SECTOR_SIZE];
    read_sector(1, bitmap);
    bitmap[block / 8] &= ~(1 << (block % 8));
    write_sector(1, bitmap);
}

// static int bitmap_test(uint32_t block) {
//     if (block >= sb.data_block_count) return -1;
//     uint8_t bitmap[AMFS_SECTOR_SIZE];
//     read_sector(1, bitmap);
//     return (bitmap[block / 8] & (1 << (block % 8))) != 0;
// }

static uint32_t bitmap_alloc(void) {
    uint8_t bitmap[AMFS_SECTOR_SIZE];
    if (read_sector(1, bitmap) != 0) return 0;
    for (uint32_t i = 0; i < sb.data_block_count; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            bitmap[i / 8] |= (1 << (i % 8));
            write_sector(1, bitmap);
            return i + 1;
        }
    }
    return 0;
}

/* --- Inode management --- */

static uint32_t inode_sector(uint32_t inum) {
    return sb.inode_start + (inum / AMFS_INODES_PER_SECTOR);
}

static uint32_t inode_offset(uint32_t inum) {
    return (inum % AMFS_INODES_PER_SECTOR) * AMFS_INODE_SIZE;
}

static void inode_read(uint32_t inum, amfs_inode_t *inode) {
    uint8_t sector[AMFS_SECTOR_SIZE];
    read_sector(inode_sector(inum), sector);
    memcpy(inode, sector + inode_offset(inum), sizeof(amfs_inode_t));
}

static void inode_write(uint32_t inum, const amfs_inode_t *inode) {
    uint8_t sector[AMFS_SECTOR_SIZE];
    read_sector(inode_sector(inum), sector);
    memcpy(sector + inode_offset(inum), inode, sizeof(amfs_inode_t));
    write_sector(inode_sector(inum), sector);
    uint8_t sec[512];
    read_sector(inode_sector(inum), sec);
}

static uint32_t inode_alloc(uint32_t type) {
    for (uint32_t i = 0; i < sb.inode_count; i++) {
        amfs_inode_t inode;
        inode_read(i, &inode);
        if (inode.type == AMFS_TYPE_FREE) {
            memset(&inode, 0, sizeof(inode));
            inode.type = type;
            inode_write(i, &inode);
            return i;
        }
    }
    return AMFS_INVALID_INODE;
}

static void inode_free(uint32_t inum) {
    amfs_inode_t inode;
    inode_read(inum, &inode);
    for (int i = 0; i < AMFS_DIRECT_BLOCKS; i++) {
        if (inode.blocks[i]) {
            bitmap_clear(inode.blocks[i] - sb.data_start);
            inode.blocks[i] = 0;
        }
    }

    if (inode.indirect) {
        uint32_t ind[AMFS_SECTOR_SIZE / 4];
        read_sector(inode.indirect, ind);
        for (uint32_t i = 0; i < AMFS_SECTOR_SIZE / 4; i++) {
            if (ind[i]) bitmap_clear(ind[i] - sb.data_start);
        }

        bitmap_clear(inode.indirect - sb.data_start);
        inode.indirect = 0;
    }

    inode.type = AMFS_TYPE_FREE;
    inode_write(inum, &inode);
}

/* --- Block mapping --- */
static uint32_t block_for_offset(uint32_t inum, size_t offset, int alloc) {
    amfs_inode_t inode;
    inode_read(inum, &inode);
    size_t bidx = offset / AMFS_SECTOR_SIZE;
    uint32_t sector = 0;
    int modified = 0;

    if (bidx < AMFS_DIRECT_BLOCKS) {
        if (alloc && !inode.blocks[bidx]) {
            uint32_t b = bitmap_alloc();
            if (!b) return 0;
            inode.blocks[bidx] = sb.data_start + (b - 1);
            modified = 1;
        }
        sector = inode.blocks[bidx];
    } else {
        bidx -= AMFS_DIRECT_BLOCKS;
        if (bidx >= (AMFS_SECTOR_SIZE / 4)) return 0;
        if (alloc && !inode.indirect) {
            uint32_t b = bitmap_alloc();
            if (!b) return 0;
            inode.indirect = sb.data_start + (b - 1);
            uint32_t zero[AMFS_SECTOR_SIZE / 4] = { 0 };
            write_sector(inode.indirect, zero);
            modified = 1;
        }
        if (!inode.indirect) return 0;
        uint32_t ind[AMFS_SECTOR_SIZE / 4];
        read_sector(inode.indirect, ind);
        if (alloc && !ind[bidx]) {
            uint32_t b = bitmap_alloc();
            if (!b) return 0;
            ind[bidx] = sb.data_start + (b - 1);
            write_sector(inode.indirect, ind);
        }
        sector = ind[bidx];
    }
    if (modified) {
        inode_write(inum, &inode);
    }
    return sector;
}

/* --- Directory operations --- */
static int dir_find_entry(uint32_t dir_inum, const char *name, amfs_dirent_t *out, uint32_t *index) {
    amfs_inode_t dir;
    inode_read(dir_inum, &dir);
    if (dir.type != AMFS_TYPE_DIR) return -1;

    uint32_t entries = dir.size / AMFS_DIRENT_SIZE;
    for (uint32_t i = 0; i < entries; i++) {
        uint32_t offset = i * AMFS_DIRENT_SIZE;
        uint32_t sector = block_for_offset(dir_inum, offset, 0);
        if (!sector) continue;
       uint8_t buf[AMFS_SECTOR_SIZE];
       read_sector(sector, buf);
        amfs_dirent_t *e = (amfs_dirent_t *)(buf + (offset % AMFS_SECTOR_SIZE));
        if (e->inode != 0 && strcmp(e->name, name) == 0) {
            if (out) memcpy(out, e, sizeof(*e));
            if (index) *index = i;
            return 0;
        }
    }
    return -1;
}

static int dir_add_entry(uint32_t dir_inum, const char *name, uint32_t inum) {
    if (strlen(name) >= AMFS_NAME_LEN) return -1;

    amfs_inode_t dir;
    inode_read(dir_inum, &dir);
    if (dir.type != AMFS_TYPE_DIR) return -1;
    if (dir_find_entry(dir_inum, name, NULL, NULL) == 0) return -1;

    uint32_t entries = dir.size / AMFS_DIRENT_SIZE;
    uint32_t slot = entries;

    for (uint32_t i = 0; i < entries; i++) {
        uint32_t offset = i * AMFS_DIRENT_SIZE;
        uint32_t sector = block_for_offset(dir_inum, offset, 0);
        if (!sector) continue;
        uint8_t buf[AMFS_SECTOR_SIZE];
        read_sector(sector, buf);
        amfs_dirent_t *e = (amfs_dirent_t *)(buf + (offset % AMFS_SECTOR_SIZE));
        if (e->inode == 0) {
            slot = i;
            break;
        }
    }

    uint32_t offset = slot * AMFS_DIRENT_SIZE;
    uint32_t sector = block_for_offset(dir_inum, offset, 1);
    if (!sector) return -1;

    uint8_t buf[AMFS_SECTOR_SIZE];
    read_sector(sector, buf);
    amfs_dirent_t *e = (amfs_dirent_t *)(buf + (offset % AMFS_SECTOR_SIZE));
    e->inode = inum;
    strncpy(e->name, name, AMFS_NAME_LEN - 1);
    e->name[AMFS_NAME_LEN - 1] = '\0';

    write_sector(sector, buf);

    if (slot == entries) {
        inode_read(dir_inum, &dir);
        dir.size += AMFS_DIRENT_SIZE;
        inode_write(dir_inum, &dir);
    }
    return 0;
}

static int dir_remove_entry(uint32_t dir_inum, const char *name) {
    amfs_dirent_t entry;
    uint32_t index;
    if (dir_find_entry(dir_inum, name, &entry, &index) != 0) {
        return -1;
    }

    uint32_t offset = index * AMFS_DIRENT_SIZE;
    uint32_t sector = block_for_offset(dir_inum, offset, 0);
    if (!sector) return -1;

    uint8_t buf[AMFS_SECTOR_SIZE];
    read_sector(sector, buf);
    amfs_dirent_t *e = (amfs_dirent_t *)(buf + (offset % AMFS_SECTOR_SIZE));
    e->inode = 0;
    e->name[0] = '\0';
    write_sector(sector, buf);
    return 0;
}

/* --- Path Resolution --- */
static int path_lookup(const char *path, uint32_t *out_inum) {
    if (!path || path[0] != '/') return -1;

    uint32_t current = sb.root_inode;
    char name[AMFS_NAME_LEN];
    const char *p = path + 1;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        int i = 0;
        while (*p && *p != '/' && i < AMFS_NAME_LEN - 1) {
            name[i++] = *p++;
        }
        name[i] = '\0';

        amfs_inode_t inode;
        inode_read(current, &inode);
        if (inode.type != AMFS_TYPE_DIR) return -1;

        amfs_dirent_t entry;
        if (dir_find_entry(current, name, &entry, NULL) != 0) {
            return -1;
        }
        current = entry.inode;
    }

    if (out_inum) *out_inum = current;
    return 0;
}

static int path_lookup_parent(const char *path, uint32_t *parent, char *name) {
    if (!path || path[0] != '/') return -1;
    if (strcmp(path, "/") == 0) return -1;

    const char *last_slash = strrchr(path, '/');
    if (!last_slash) return -1;

    const char *name_start = last_slash + 1;
    while (*name_start == '/') name_start++;
    if (*name_start == '\0') return -1;

    if (last_slash == path) {
        if (parent) *parent = sb.root_inode;
        if (name) {
            strncpy(name, name_start, AMFS_NAME_LEN - 1);
            name[AMFS_NAME_LEN - 1] = '\0';
        }
        return 0;
    }

    char parent_path[128];
    int len = last_slash - path;
    if (len > 128) return -1;
    memcpy(parent_path, path, len);
    parent_path[len] = '\0';

    if (path_lookup(parent_path, parent) != 0) return -1;

    if (name) {
        strncpy(name, name_start, AMFS_NAME_LEN - 1);
        name[AMFS_NAME_LEN - 1] = '\0';
    }
    return 0;
}

/* --- Public API --- */
int amfs_mkfs(uint32_t total_sectors) {
    if (total_sectors < 12) return -1;

    memset(&sb, 0, sizeof(sb));
    memcpy(sb.magic, AMFS_MAGIC, 4);
    sb.version = AMFS_VERSION;
    sb.total_sectors = total_sectors;
    sb.inode_count = AMFS_MAX_INODES;
    sb.inode_start = 2;
    sb.data_start = 2 + AMFS_INODE_SECTORS;
    sb.data_block_count = total_sectors - sb.data_start;
    sb.block_size = AMFS_SECTOR_SIZE;
    sb.root_inode = 0;

    uint8_t sec[AMFS_SECTOR_SIZE];

    memset(sec, 0, sizeof(sec));
    memcpy(sec, &sb, sizeof(sb));


    if (write_sector(0, &sb) != 0) return -1;
    uint8_t zero[AMFS_SECTOR_SIZE];
    memset(zero, 0, AMFS_SECTOR_SIZE);
    write_sector(1, zero);

    for (uint32_t i = 0; i < AMFS_INODE_SECTORS; i++) {
        write_sector(sb.inode_start + i, zero);
    }

    amfs_inode_t root;
    memset(&root, 0, sizeof(root));
    root.type = AMFS_TYPE_DIR;
    root.parent = 0;
    inode_write(0, &root);
    amfs_inode_t test;
    inode_read(0, &test);

    printk("[AMFS] Formatted %d sectors, %d data blocks\n", total_sectors, sb.data_block_count);
    return 0;
}

int amfs_mount(void) {
    uint8_t sec[AMFS_SECTOR_SIZE];

    read_sector(0, sec);
    memcpy(&sb, sec, sizeof(sb));
    if (read_sector(0, sec) != 0) {
        printk("[AMFS] failed to read superblock\n");
        return -1;
    }
    if (memcmp(sb.magic, AMFS_MAGIC, 4) != 0) {
        printk("[AMFS] Magic mismatch\n");
        return -1;
    }
    if (sb.version != AMFS_VERSION) {
        printk("[AMFS] Version mismatch: expected %d, got %d\n", AMFS_VERSION, sb.version);
        return -1;
    }
    mounted = 1;
    printk("[AMFS] Mounted, %d inodes, %d data blocks", sb.inode_count, sb.data_block_count);

    amfs_inode_t inode;

    inode_read(0, &inode);
    inode_read(1, &inode);
    inode_read(2, &inode);

    return 0;
}

int amfs_mkdir(const char *path) {
    /* The fuck up machine: */
    if (!mounted) return -1;                      /* Can't create directory if you're not using the file system*/
    if (!path || path[0] != '/') return -1;       /* Give a path at least */
    if (strcmp(path, "/") == 0) return -1; /* You can't create root */
    if (amfs_exists(path)) return -1;             /* You already made this path */

    uint32_t parent;
    char name[AMFS_NAME_LEN];
    if (path_lookup_parent(path, &parent, name) != 0) return -1; /* Path should have a parent, at least root */

    uint32_t inum = inode_alloc(AMFS_TYPE_DIR); /* Make directory inode */
    if (inum == AMFS_INVALID_INODE) return -1;                             /* Congrats, you messed up again */

    amfs_inode_t inode;
    memset(&inode, 0, sizeof(inode)); /* "How to Calloc a stack variable" */
    inode.type = AMFS_TYPE_DIR;
    /* It seems monospace has stopped working, but just in this file */
    inode.parent = parent;
    inode_write(inum, &inode);

    if (dir_add_entry(parent, name, inum) != 0) {
        inode_free(inum);
        return -1;
    }

    printk("[AMFS] Created directory '%s'\n", path);
    return 0;
}

int amfs_create(const char *path) {
    /* The fuck up machine comes again:*/
    if (!mounted) return -1;
    if (!path || path[0] != '/') return -1;
    if (strcmp(path, "/") == 0) return -1;
    if (amfs_exists(path)) return -1;

    uint32_t parent;
    char name[AMFS_NAME_LEN];
    if (path_lookup_parent(path, &parent, name) != 0) return -1;

    uint32_t inum = inode_alloc(AMFS_TYPE_FILE);
    if (inum == AMFS_INVALID_INODE) return -1;

    amfs_inode_t inode;
    memset(&inode, 0, sizeof(inode));
    inode.type = AMFS_TYPE_FILE;
    inode.parent = parent;
    inode_write(inum, &inode);
    if (dir_add_entry(parent, name, inum) != 0) {
        inode_free(inum);
        return -1;
    }
    return 0;
}

int amfs_write(const char *path, const char *data, size_t size) {
    if (!mounted) return -1;
    if (!path || !data || size == 0) return -1;

    uint32_t inum;
    if (path_lookup(path, &inum) != 0) {
        if (amfs_create(path) != 0) return -1; /* If the file we want to write to doesn't exist yet, then create it */
        if (path_lookup(path, &inum) != 0) return -1; /** Check if file exists */
    }

    amfs_inode_t inode;
    inode_read(inum, &inode);
    if (inode.type != AMFS_TYPE_FILE) return -1;
    /* Truncate existing file */
    for (int i = 0; i < AMFS_DIRECT_BLOCKS; i++) {
        if (inode.blocks[i]) {
            bitmap_clear(inode.blocks[i] - sb.data_start);
            inode.blocks[i] = 0;
        }
    }
    if (inode.indirect) {
        uint32_t ind[AMFS_SECTOR_SIZE / 4];
        read_sector(inode.indirect, ind);
        for (uint32_t i = 0; i < AMFS_SECTOR_SIZE / 4; i++) {
            if (ind[i]) bitmap_clear(ind[i] - sb.data_start);
        }
        bitmap_clear(inode.indirect - sb.data_start);
        inode.indirect = 0;
    }
    inode.size = 0;
    inode_write(inum, &inode);

    /* Write new data */
    for (size_t offset = 0; offset < size; offset += AMFS_SECTOR_SIZE) {
        uint32_t sector = block_for_offset(inum, offset, 1);
        if (!sector) return -1;

        uint8_t buf[AMFS_SECTOR_SIZE];
        memset(buf, 0, AMFS_SECTOR_SIZE);
        size_t to_copy = size - offset;
        if (to_copy > AMFS_SECTOR_SIZE) to_copy = AMFS_SECTOR_SIZE;
        memcpy(buf, data + offset, to_copy);
        write_sector(sector, buf);
    }

    inode_read(inum, &inode);
    inode.size = size;
    inode_write(inum, &inode);

    printk("[AMFS] wrote '%s' (%d bytes)\n", path, size);
    return 0;
}
int amfs_read(const char *path, char *buf, size_t buf_size) {
    if (!mounted) return -1;
    if (!path || !buf || buf_size == 0) return -1;

    uint32_t inum;
    if (path_lookup(path, &inum) != 0) return -1;

    amfs_inode_t inode;
    inode_read(inum, &inode);
    if (inode.type != AMFS_TYPE_FILE) return -1;

    size_t to_read = inode.size;
    if (to_read > buf_size) to_read = buf_size;

    for (size_t offset = 0; offset < to_read; offset += AMFS_SECTOR_SIZE) {
        uint32_t sector = block_for_offset(inum, offset, 0);
        if (!sector) return -1;

        uint8_t sector_buf[AMFS_SECTOR_SIZE];
        read_sector(sector, sector_buf);

        size_t chunk = to_read - offset;
        if (chunk > AMFS_SECTOR_SIZE) chunk = AMFS_SECTOR_SIZE;
        memcpy(buf + offset, sector_buf, chunk);
    }
    return to_read;
}

int amfs_delete(const char *path) {
    if (!mounted) return 0;
    if (!path || path[0] != '/') return -1;
    if (strcmp(path, "/") == 0) return -1;

    uint32_t parent;
    char name[AMFS_NAME_LEN];
    if (path_lookup_parent(path, &parent, name) != 0) return -1;

    uint32_t inum;
    if (path_lookup(path, &inum) != 0) return -1;

    amfs_inode_t inode;
    inode_read(inum, &inode);

    if (inode.type == AMFS_TYPE_DIR) {
        uint32_t entries = inode.size / AMFS_DIRENT_SIZE;
        for (uint32_t i = 0; i < entries; i++) {
            uint32_t offset = i * AMFS_DIRENT_SIZE;
            uint32_t sector = block_for_offset(inum, offset, 0);
            if (!sector) continue;
            uint8_t buf[AMFS_SECTOR_SIZE];
            read_sector(sector, buf);
            amfs_dirent_t *e = (amfs_dirent_t *)(buf + (offset % AMFS_SECTOR_SIZE));
            if (e->inode != 0) {
                printk("[AMFS] directory not empty\n");
                return -1;
            }
        }
    }

    inode_free(inum);
    dir_remove_entry(parent, name);

    printk("[AMFS] Deleted '%s'\n", path);
    return 0;
}

void amfs_ls(const char *path) {
    if (!mounted) {
        printk("[AMFS] Not mounted\n");
        return;
    }

    uint32_t inum;
    if (!path || path[0] != '/') {
        inum = sb.root_inode;
    } else {
        if (path_lookup(path, &inum) != 0) {
            printk("[AMFS] '%s' not found\n", path);
            return;
        }
    }

    amfs_inode_t inode;
    inode_read(inum, &inode);
    if (inode.type != AMFS_TYPE_DIR) {
        printk("[AMFS] '%s' is not a directory\n", path);
        return;
    }

    printk("\n=== %s ===\n", path ? path : "/");

    uint32_t entries = inode.size / AMFS_DIRENT_SIZE;
    int count = 0;

    for (uint32_t i = 0; i < entries; i++) {
        uint32_t offset = i * AMFS_DIRENT_SIZE;
        uint32_t sector = block_for_offset(inum, offset, 0);
        if (!sector) continue;

        uint8_t buf[AMFS_SECTOR_SIZE];
        read_sector(sector, buf);
        amfs_dirent_t *e = (amfs_dirent_t *)(buf + (offset % AMFS_SECTOR_SIZE));;

        if (e->inode != 0) {
            amfs_inode_t child;
            inode_read(e->inode, &child);
            const char *type = (child.type == AMFS_TYPE_DIR) ? "DIR" : "FILE";
            printk("  %-28s %s %6d bytes\n", e->name, type, child.size);
            count++;
        }
    }

    printk("Total: %d entries\n\n", count);
}

int amfs_exists(const char *path) {
    return path_lookup(path, NULL) == 0;
}

int amfs_is_mounted(void) {
    return mounted;
}

void amfs_cat(const char *path) {
    char buf[4096];
    int len;

    len = amfs_read(path, buf, sizeof(buf));
    if (len > 0) {
        buf[len] = '\0';
        printk("[AMFS] Read back on %s:\n\n%s\n", path, buf);
    }
}