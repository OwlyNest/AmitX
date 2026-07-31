/*
	* fs/smkfs/file.c - File I/O and File Descriptor Management (G1)
	* Author:   amity
	* Date:     Wed Jul 29 17:38:56 2026
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
#include <fs/smkfs.h>
#include <fs/smkfs_internal.h>
#include <lib/string.h>
#include <screen/printk.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

int smkfs_read(smkfs_mount_t *mnt, uint64_t record_id, uint64_t offset, size_t len, void *buf) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *fsize_ptr;
    uint64_t file_size;
    size_t to_read;
    uint8_t *out = (uint8_t *)buf;

    if (!mnt->mounted || !buf || record_id == 0) return SMKFS_ERR_INVAL;
    if (record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
    }
    
    if (rec.object_type != SMKFS_ROT_FILE) return SMKFS_ERR_INVAL;
    if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (void **)&fsize_ptr, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
    }

    file_size = *fsize_ptr;
    if (offset >= file_size) return 0;
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

        if (extent_resolve(mnt, record_id, logical_block, &ext) != 0) {
            return SMKFS_ERR_NOTFOUND;
        }

        if (read_block(mnt, ext.physical_block + (logical_block - ext.logical_offset), block) != 0) {
            return SMKFS_ERR_IO;
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

int smkfs_write(smkfs_mount_t *mnt, uint64_t record_id, uint64_t offset, size_t len, const void *buf) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *fsize_ptr;
    uint64_t file_size;
    uint64_t new_size;
    const uint8_t *in = (uint8_t *)buf;

    if (!mnt->mounted || !buf || record_id == 0) return SMKFS_ERR_INVAL;
    if (record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
    }

    if (rec.object_type != SMKFS_ROT_FILE) return SMKFS_ERR_INVAL;
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

        if (extent_resolve(mnt, record_id, logical_block, &ext) == 0) {
            phys_block = ext.physical_block + (logical_block - ext.logical_offset);
            if (read_block(mnt, phys_block, block) != 0) {
                return SMKFS_ERR_IO;
            }
        } else {
            phys_block = bitmap_alloc(mnt);
            if (phys_block == 0) return SMKFS_ERR_NOSPC;
            if (extent_add(mnt, record_id, logical_block, phys_block, 1) != 0) {
                bitmap_clear(mnt, phys_block);
                return SMKFS_ERR_NOSPC;
            }

            memset(block, 0, sizeof(block));
        }

        chunk = len - done;
        if (chunk > SMKFS_BLOCK_SIZE - block_offset) {
            chunk = SMKFS_BLOCK_SIZE - block_offset;
        }

        memcpy(block + block_offset, in + done, chunk);
        if (write_block(mnt, phys_block, block) != 0) {
            return SMKFS_ERR_IO;
        }

        done += chunk;
    }

    if (new_size != file_size) {
        uint8_t final_attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
        smkfs_record_t final_rec;

        if (record_read(mnt, record_id, &final_rec, final_attr_buf, sizeof(final_attr_buf)) < 0) {
            return SMKFS_ERR_IO;
        }

        record_add_attr(final_attr_buf, sizeof(final_attr_buf), SMKFS_ATTRT_FSIZE, &new_size, sizeof(new_size));
        if (record_write(mnt, record_id, &final_rec, final_attr_buf) != 0) {
            return SMKFS_ERR_IO;
        }
    }

    return (int)len;
}

int smkfs_truncate(smkfs_mount_t *mnt, uint64_t record_id, uint64_t new_size) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *fsize_ptr;
    uint64_t old_size;
    uint64_t old_blocks;
    uint64_t new_blocks;

    if (!mnt->mounted || record_id == 0) return SMKFS_ERR_INVAL;
    if (record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
    }

    if (rec.object_type != SMKFS_ROT_FILE) return SMKFS_ERR_INVAL;
    if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (void **)&fsize_ptr, NULL) != 0) {
        old_size = 0;
    } else {
        old_size = *fsize_ptr;
    }

    if (new_size == old_size) return SMKFS_OK;

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
                        bitmap_free_range(mnt, ext[i].physical_block, ext[i].block_count);
                        ext[i].block_count = 0;
                    } else if (ext_end > new_blocks) {
                        uint64_t keep = new_blocks - ext[i].logical_offset;
                        bitmap_free_range(mnt, ext[i].physical_block + keep, ext[i].block_count - keep);
                        ext[i].block_count = (uint32_t)keep;
                    }
                }
                record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_EXTENTS, ext, num * sizeof(smkfs_extent_t));
            }
        }
    }

    record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_FSIZE, &new_size, sizeof(new_size));
    rec.attr_count++;
    rec.header.length = sizeof(smkfs_record_t) + attr_buf_total_len(attr_buf);
    return record_write(mnt, record_id, &rec, attr_buf);
}

int smkfs_open(smkfs_mount_t *mnt, const char *path, int flags) {
    uint64_t record_id;
    int fd;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    if (path_lookup(mnt, path, &record_id) != 0) {
        if (flags & SMKFS_O_CREATE) {
            if (smkfs_create_file(mnt, path, SMKFS_PERM_WRITE | SMKFS_PERM_WRITE) != 0) {
                return SMKFS_ERR_NOSPC;
            }

            if (path_lookup(mnt, path, &record_id) != 0) {
                return SMKFS_ERR_NOTFOUND;
            }
        } else {
            return SMKFS_ERR_NOTFOUND;
        }
    }

    smkfs_dump_record(mnt, record_id);

    for (fd = 0; fd < SMKFS_FD_MAX; fd++) {
        if (!mnt->fd_table[fd].used) break;
    }

    if (fd >= SMKFS_FD_MAX) return SMKFS_ERR_NOSPC;

    mnt->fd_table[fd].used = 1;
    mnt->fd_table[fd].record_id = record_id;
    mnt->fd_table[fd].offset = 0;
    mnt->fd_table[fd].flags = flags;

    if (flags & SMKFS_O_APPEND) {
        uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
        smkfs_record_t rec;
        uint64_t *fsize_ptr;
        if (record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf)) == 0) {
            if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (void **)&fsize_ptr, NULL) == 0) {
                mnt->fd_table[fd].offset = *fsize_ptr;
            }
        }
    }

    printk("[OPEN] fd: %d\n", fd);
    return fd;
}

int smkfs_close(smkfs_mount_t *mnt, int fd) {
    if (fd < 0 || fd >= SMKFS_FD_MAX) return SMKFS_ERR_INVAL;
    if (!mnt->fd_table[fd].used) return SMKFS_ERR_INVAL;

    mnt->fd_table[fd].used = 0;
    mnt->fd_table[fd].record_id = 0;
    mnt->fd_table[fd].offset = 0;
    mnt->fd_table[fd].flags = 0;
    return SMKFS_OK;
}

int smkfs_read_file(smkfs_mount_t *mnt, int fd, void *buf, size_t len) {
    int ret;

    if (fd < 0 || fd >= SMKFS_FD_MAX) return SMKFS_ERR_INVAL;
    if (!mnt->fd_table[fd].used) return SMKFS_ERR_INVAL;
    if (!buf) return SMKFS_ERR_INVAL;

    ret = smkfs_read(mnt, mnt->fd_table[fd].record_id, mnt->fd_table[fd].offset, len, buf);
    if (ret > 0) mnt->fd_table[fd].offset += ret;
    return ret;
}

int smkfs_write_file(smkfs_mount_t *mnt, int fd, const void *buf, size_t len) {
    int ret;

    if (fd < 0 || fd >= SMKFS_FD_MAX) return SMKFS_ERR_INVAL;
    if (!mnt->fd_table[fd].used) return SMKFS_ERR_INVAL;
    if (!buf) return SMKFS_ERR_INVAL;

    ret = smkfs_write(mnt, mnt->fd_table[fd].record_id, mnt->fd_table[fd].offset, len, buf);
    if (ret > 0) mnt->fd_table[fd].offset += ret;
    return ret;
}

int smkfs_seek(smkfs_mount_t *mnt, int fd, int64_t offset, int whence) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *fsize_ptr;
    uint64_t file_size = 0;
    int64_t new_offset;

    if (fd < 0 || fd >= SMKFS_FD_MAX) return SMKFS_ERR_INVAL;
    if (!mnt->fd_table[fd].used) return SMKFS_ERR_INVAL;

    switch (whence) {
    case SMKFS_SEEK_END:
        if (record_read(mnt, mnt->fd_table[fd].record_id, &rec, attr_buf, sizeof(attr_buf)) == 0) {
            if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (void **)&fsize_ptr, NULL) == 0) {
                file_size = *fsize_ptr;
            }
        }

        new_offset = (int64_t)file_size + offset;
        break;
    case SMKFS_SEEK_CUR:
        new_offset = (int64_t)mnt->fd_table[fd].offset + offset;
        break;
    case SMKFS_SEEK_SET:
        new_offset = offset;
        break;
    default:
        return SMKFS_ERR_INVAL;
    }

    if (new_offset < 0) return SMKFS_ERR_INVAL;
    mnt->fd_table[fd].offset = (uint64_t)new_offset;
    return (int)mnt->fd_table[fd].offset;
}

int smkfs_create_file(smkfs_mount_t *mnt, const char *path, uint16_t permissions) {
    uint64_t parent;
    char name[SMKFS_NAME_LEN];
    const char *last_slash;
    const char *name_start;
    uint64_t new_record;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    last_slash = strrchr(path, '/');
    if (!last_slash || last_slash == path + 2) {
        parent = mnt->sb.root_record_id;
        name_start = path + 3;
    } else {
        char parent_path[SMKFS_NAME_LEN];
        int len = last_slash - path;
        if (len >= SMKFS_NAME_LEN) return SMKFS_ERR_TOO_BIG;
        memcpy(parent_path, path, len);
        parent_path[len] = '\0';
        if (path_lookup(mnt, parent_path, &parent) != 0) {
            return SMKFS_ERR_NOTFOUND;
        }

        name_start = last_slash + 1;
    }

    int i = 0;
    while (*name_start && *name_start != '/' && i < SMKFS_NAME_LEN - 1) {
        name[i++] = *name_start++;
    }

    name[i] = '\0';

    if (smkfs_create_record(mnt, SMKFS_ROT_FILE, parent, name, &new_record)!= 0) {
        return SMKFS_ERR_NOSPC;
    }

    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    if (record_read(mnt, new_record, &rec, attr_buf, sizeof(attr_buf)) == 0) {
        record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_PERMISSIONS, &permissions, sizeof(permissions));
        rec.attr_count++;
        record_write(mnt, new_record, &rec, attr_buf);
    }
    return SMKFS_OK;
}

int smkfs_delete_file(smkfs_mount_t *mnt, const char *path) {
    uint64_t record_id;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    if (path_lookup(mnt, path, &record_id) != 0) {
        return SMKFS_ERR_NOTFOUND;
    }

    return smkfs_delete_record(mnt, record_id);
}

int smkfs_mkdir(smkfs_mount_t *mnt, const char *path) {
    uint64_t parent;
    char name[SMKFS_NAME_LEN];
    const char *last_slash;
    const char *name_start;
    uint64_t new_record;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    last_slash = strrchr(path, '/');
    if (!last_slash || last_slash == path + 2) {
        parent = mnt->sb.root_record_id;
        name_start = path + 3;
    } else {
        char parent_path[SMKFS_NAME_LEN];
        int len = last_slash - path;
        if (len >= SMKFS_NAME_LEN) return SMKFS_ERR_TOO_BIG;
        memcpy(parent_path, path, len);
        parent_path[len] = '\0';
        if (path_lookup(mnt, parent_path, &parent) != 0) {
            return SMKFS_ERR_NOTFOUND;
        }

        name_start = last_slash + 1;
    }

    int i = 0;
    while (*name_start && *name_start != '/' && i < SMKFS_NAME_LEN - 1) {
        name[i++] = *name_start++;
    }

    name[i] = '\0';

    return smkfs_create_record(mnt, SMKFS_ROT_DIR, parent, name, &new_record);
}

int smkfs_rmdir(smkfs_mount_t *mnt, const char *path) {
    uint64_t record_id;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    if (path_lookup(mnt, path, &record_id) != 0) {
        return SMKFS_ERR_NOTFOUND;
    }

    return smkfs_delete_record(mnt, record_id);
}