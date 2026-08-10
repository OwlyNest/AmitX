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

LONG smkfs_read(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, SMKFS_OFFSET offset, SIZE_T len, PVOID buf) {
    UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
    _SMKFS_RECORD rec;
    ULONGLONG *fsize_ptr;
    ULONGLONG file_size;
    SIZE_T to_read;
    PUCHAR out = (PUCHAR)buf;

    if (!mnt->mounted || !buf || record_id == 0) return SMKFS_ERR_INVAL;
    if (record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
    }
    
    if (rec.object_type != SMKFS_ROT_FILE) return SMKFS_ERR_INVAL;
    if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (PVOID *)&fsize_ptr, NULL) != SMKFS_OK) {
        return SMKFS_ERR_NOTFOUND;
    }

    file_size = *fsize_ptr;
    if (offset >= file_size) return 0;
    to_read = len;
    if (offset + to_read > file_size) {
        to_read = (SIZE_T)(file_size - offset);
    }

    for (SIZE_T done = 0; done < to_read; ) {
        SMKFS_LBLOCK logical_block = (offset + done) / SMKFS_BLOCK_SIZE;
        SIZE_T block_offset = (offset + done) % SMKFS_BLOCK_SIZE;
        _SMKFS_EXTENT ext;
        UCHAR block[SMKFS_BLOCK_SIZE];
        SIZE_T chunk;

        if (extent_resolve(mnt, record_id, logical_block, &ext) != SMKFS_OK) {
            return SMKFS_ERR_NOTFOUND;
        }

        if (read_block(mnt, ext.physical_block + (logical_block - ext.logical_offset), block) != SMKFS_OK) {
            return SMKFS_ERR_IO;
        }

        chunk = to_read - done;
        if (chunk > SMKFS_BLOCK_SIZE - block_offset) {
            chunk = SMKFS_BLOCK_SIZE - block_offset;
        }

        memcpy(out + done, block + block_offset, chunk);
        done += chunk;
    }

    return (LONG)to_read;
}

LONG smkfs_write(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, SMKFS_OFFSET offset, SIZE_T len, PCVOID buf) {
    UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
    _SMKFS_RECORD rec;
    ULONGLONG *fsize_ptr;
    ULONGLONG file_size;
    ULONGLONG new_size;
    PCUCHAR in = (PCUCHAR)buf;

    if (!mnt->mounted || !buf || record_id == 0) return SMKFS_ERR_INVAL;
    if (record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
    }

    if (rec.object_type != SMKFS_ROT_FILE) return SMKFS_ERR_INVAL;
    if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (PVOID *)&fsize_ptr, NULL) != SMKFS_OK) {
        file_size = 0;
    } else {
        file_size = *fsize_ptr;
    }

    new_size = offset + len;
    if (new_size < file_size) new_size = file_size;

    for (SIZE_T done = 0; done < len; ) {
        SMKFS_LBLOCK logical_block = (offset + done) / SMKFS_BLOCK_SIZE;
        SIZE_T block_offset = (offset + done) % SMKFS_BLOCK_SIZE;
        _SMKFS_EXTENT ext;
        UCHAR block[SMKFS_BLOCK_SIZE];
        SIZE_T chunk;
        SMKFS_BLOCK phys_block;

        if (extent_resolve(mnt, record_id, logical_block, &ext) == SMKFS_OK) {
            phys_block = ext.physical_block + (logical_block - ext.logical_offset);
            if (read_block(mnt, phys_block, block) != SMKFS_OK) {
                return SMKFS_ERR_IO;
            }
        } else {
            phys_block = bitmap_alloc(mnt);
            if (phys_block == 0) return SMKFS_ERR_NOSPC;
            if (extent_add(mnt, record_id, logical_block, phys_block, 1) != SMKFS_OK) {
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
        if (write_block(mnt, phys_block, block) != SMKFS_OK) {
            return SMKFS_ERR_IO;
        }

        done += chunk;
    }

    if (new_size != file_size) {
        UCHAR final_attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
        _SMKFS_RECORD final_rec;

        if (record_read(mnt, record_id, &final_rec, final_attr_buf, sizeof(final_attr_buf)) < 0) {
            return SMKFS_ERR_IO;
        }

        if (record_add_attr(final_attr_buf, sizeof(final_attr_buf), SMKFS_ATTRT_FSIZE, &new_size, sizeof(new_size)) == SMKFS_OK) {
            final_rec.attr_count++;
            if (record_write(mnt, record_id, &final_rec, final_attr_buf) != SMKFS_OK) {
                return SMKFS_ERR_IO;
            }
        }
    }

    return (LONG)len;
}

SMKFS_STATUS smkfs_truncate(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, ULONGLONG new_size) {
    UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
    _SMKFS_RECORD rec;
    ULONGLONG *fsize_ptr;
    ULONGLONG old_size;
    ULONGLONG old_blocks;
    ULONGLONG new_blocks;

    if (!mnt->mounted || record_id == 0) return SMKFS_ERR_INVAL;
    if (record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
    }

    if (rec.object_type != SMKFS_ROT_FILE) return SMKFS_ERR_INVAL;
    if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (PVOID *)&fsize_ptr, NULL) != SMKFS_OK) {
        old_size = 0;
    } else {
        old_size = *fsize_ptr;
    }

    if (new_size == old_size) return SMKFS_OK;

    if (new_size < old_size) {
        old_blocks = (old_size + SMKFS_BLOCK_SIZE - 1) / SMKFS_BLOCK_SIZE;
        new_blocks = (new_size + SMKFS_BLOCK_SIZE - 1) / SMKFS_BLOCK_SIZE;

        if (new_blocks < old_blocks) {
            PVOID ext_data;
            SIZE_T ext_len;
            if (record_find_attr(attr_buf, SMKFS_ATTRT_EXTENTS, &ext_data, &ext_len) == SMKFS_OK) {
                ULONG num = ext_len / sizeof(_SMKFS_EXTENT);
                _SMKFS_EXTENT *ext = (_SMKFS_EXTENT *)ext_data;
                for (ULONG i = 0; i < num; i++) {
                    ULONGLONG ext_end = ext[i].logical_offset + ext[i].block_count;
                    if (ext[i].logical_offset >= new_blocks) {
                        bitmap_free_range(mnt, ext[i].physical_block, ext[i].block_count);
                        ext[i].block_count = 0;
                    } else if (ext_end > new_blocks) {
                        ULONGLONG keep = new_blocks - ext[i].logical_offset;
                        bitmap_free_range(mnt, ext[i].physical_block + keep, ext[i].block_count - (ULONG)keep);
                        ext[i].block_count = (ULONG)keep;
                    }
                }
                record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_EXTENTS, ext, num * sizeof(_SMKFS_EXTENT));
            }
        }
    }

    if (record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_FSIZE, &new_size, sizeof(new_size)) == SMKFS_OK) {
        rec.attr_count++;
        rec.header.length = sizeof(_SMKFS_RECORD) + attr_buf_total_len(attr_buf);
        return record_write(mnt, record_id, &rec, attr_buf);
    }

    return SMKFS_ERR_NOSPC;
}

LONG smkfs_open(_SMKFS_MOUNT *mnt, SMKFS_PATH path, LONG flags) {
    SMKFS_RECORD_ID record_id;
    LONG fd;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    if (path_lookup(mnt, path, &record_id) != SMKFS_OK) {
        if (flags & SMKFS_O_CREATE) {
            if (smkfs_create_file(mnt, path, SMKFS_PERM_WRITE | SMKFS_PERM_WRITE) != SMKFS_OK) {
                return SMKFS_ERR_NOSPC;
            }

            if (path_lookup(mnt, path, &record_id) != SMKFS_OK) {
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
        UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
        _SMKFS_RECORD rec;
        ULONGLONG *fsize_ptr;
        if (record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf)) >= 0) {
            if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (PVOID *)&fsize_ptr, NULL) == SMKFS_OK) {
                mnt->fd_table[fd].offset = *fsize_ptr;
            }
        }
    }

    printk("[OPEN] fd: %d\n", fd);
    return fd;
}

SMKFS_STATUS smkfs_close(_SMKFS_MOUNT *mnt, LONG fd) {
    if (fd < 0 || fd >= SMKFS_FD_MAX) return SMKFS_ERR_INVAL;
    if (!mnt->fd_table[fd].used) return SMKFS_ERR_INVAL;

    mnt->fd_table[fd].used = 0;
    mnt->fd_table[fd].record_id = 0;
    mnt->fd_table[fd].offset = 0;
    mnt->fd_table[fd].flags = 0;
    return SMKFS_OK;
}

LONG smkfs_read_file(_SMKFS_MOUNT *mnt, LONG fd, PVOID buf, SIZE_T len) {
    LONG ret;

    if (fd < 0 || fd >= SMKFS_FD_MAX) return SMKFS_ERR_INVAL;
    if (!mnt->fd_table[fd].used) return SMKFS_ERR_INVAL;
    if (!buf) return SMKFS_ERR_INVAL;

    ret = smkfs_read(mnt, mnt->fd_table[fd].record_id, mnt->fd_table[fd].offset, len, buf);
    if (ret > 0) mnt->fd_table[fd].offset += ret;
    return ret;
}

LONG smkfs_write_file(_SMKFS_MOUNT *mnt, LONG fd, PCVOID buf, SIZE_T len) {
    LONG ret;

    if (fd < 0 || fd >= SMKFS_FD_MAX) return SMKFS_ERR_INVAL;
    if (!mnt->fd_table[fd].used) return SMKFS_ERR_INVAL;
    if (!buf) return SMKFS_ERR_INVAL;

    ret = smkfs_write(mnt, mnt->fd_table[fd].record_id, mnt->fd_table[fd].offset, len, buf);
    if (ret > 0) mnt->fd_table[fd].offset += ret;
    return ret;
}

LONG smkfs_seek(_SMKFS_MOUNT *mnt, LONG fd, LONGLONG offset, LONG whence) {
    UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
    _SMKFS_RECORD rec;
    ULONGLONG *fsize_ptr;
    ULONGLONG file_size = 0;
    LONGLONG new_offset;

    if (fd < 0 || fd >= SMKFS_FD_MAX) return SMKFS_ERR_INVAL;
    if (!mnt->fd_table[fd].used) return SMKFS_ERR_INVAL;

    switch (whence) {
    case SMKFS_SEEK_END:
        if (record_read(mnt, mnt->fd_table[fd].record_id, &rec, attr_buf, sizeof(attr_buf)) >= 0) {
            if (record_find_attr(attr_buf, SMKFS_ATTRT_FSIZE, (PVOID *)&fsize_ptr, NULL) == SMKFS_OK) {
                file_size = *fsize_ptr;
            }
        }

        new_offset = (LONGLONG)file_size + offset;
        break;
    case SMKFS_SEEK_CUR:
        new_offset = (LONGLONG)mnt->fd_table[fd].offset + offset;
        break;
    case SMKFS_SEEK_SET:
        new_offset = offset;
        break;
    default:
        return SMKFS_ERR_INVAL;
    }

    if (new_offset < 0) return SMKFS_ERR_INVAL;
    mnt->fd_table[fd].offset = (ULONGLONG)new_offset;
    return (LONG)mnt->fd_table[fd].offset;
}

SMKFS_STATUS smkfs_create_file(_SMKFS_MOUNT *mnt, SMKFS_PATH path, SMKFS_PERM permissions) {
    SMKFS_RECORD_ID parent;
    CHAR name[SMKFS_NAME_LEN];
    PCCHAR last_slash;
    PCCHAR name_start;
    SMKFS_RECORD_ID new_record;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    last_slash = strrchr(path, '/');
    if (!last_slash || last_slash == path + 2) {
        parent = mnt->sb.root_record_id;
        name_start = path + 3;
    } else {
        CHAR parent_path[SMKFS_NAME_LEN];
        LONG len = last_slash - path;
        if (len >= SMKFS_NAME_LEN) return SMKFS_ERR_TOO_BIG;
        memcpy(parent_path, path, len);
        parent_path[len] = '\0';
        if (path_lookup(mnt, parent_path, &parent) != SMKFS_OK) {
            return SMKFS_ERR_NOTFOUND;
        }

        name_start = last_slash + 1;
    }

    LONG i = 0;
    while (*name_start && *name_start != '/' && i < SMKFS_NAME_LEN - 1) {
        name[i++] = *name_start++;
    }

    name[i] = '\0';

    if (smkfs_create_record(mnt, SMKFS_ROT_FILE, parent, name, &new_record) != SMKFS_OK) {
        return SMKFS_ERR_NOSPC;
    }

    UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
    _SMKFS_RECORD rec;
    if (record_read(mnt, new_record, &rec, attr_buf, sizeof(attr_buf)) >= 0) {
        record_add_attr(attr_buf, sizeof(attr_buf), SMKFS_ATTRT_PERMISSIONS, &permissions, sizeof(permissions));
        rec.attr_count++;
        record_write(mnt, new_record, &rec, attr_buf);
    }
    return SMKFS_OK;
}

SMKFS_STATUS smkfs_delete_file(_SMKFS_MOUNT *mnt, SMKFS_PATH path) {
    SMKFS_RECORD_ID record_id;
    SMKFS_RECORD_ID parent_id;
    CHAR name[SMKFS_NAME_LEN];

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    if (path_lookup(mnt, path, &record_id) != SMKFS_OK) {
        return SMKFS_ERR_NOTFOUND;
    }

    if (path_split(mnt, path, &parent_id, name) != SMKFS_OK) {
        return SMKFS_ERR_INVAL;
    }

    return smkfs_delete_record(mnt, parent_id, name, record_id);
}

SMKFS_STATUS smkfs_mkdir(_SMKFS_MOUNT *mnt, SMKFS_PATH path) {
    SMKFS_RECORD_ID parent;
    CHAR name[SMKFS_NAME_LEN];
    PCCHAR last_slash;
    PCCHAR name_start;
    SMKFS_RECORD_ID new_record;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    last_slash = strrchr(path, '/');
    if (!last_slash || last_slash == path + 2) {
        parent = mnt->sb.root_record_id;
        name_start = path + 3;
    } else {
        CHAR parent_path[SMKFS_NAME_LEN];
        LONG len = last_slash - path;
        if (len >= SMKFS_NAME_LEN) return SMKFS_ERR_TOO_BIG;
        memcpy(parent_path, path, len);
        parent_path[len] = '\0';
        if (path_lookup(mnt, parent_path, &parent) != SMKFS_OK) {
            return SMKFS_ERR_NOTFOUND;
        }

        name_start = last_slash + 1;
    }

    LONG i = 0;
    while (*name_start && *name_start != '/' && i < SMKFS_NAME_LEN - 1) {
        name[i++] = *name_start++;
    }

    name[i] = '\0';

    return smkfs_create_record(mnt, SMKFS_ROT_DIR, parent, name, &new_record);
}

SMKFS_STATUS smkfs_rmdir(_SMKFS_MOUNT *mnt, SMKFS_PATH path) {
    SMKFS_RECORD_ID record_id;
    SMKFS_RECORD_ID parent_id;
    CHAR name[SMKFS_NAME_LEN];

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    if (path_lookup(mnt, path, &record_id) != SMKFS_OK) {
        return SMKFS_ERR_NOTFOUND;
    }

    if (path_split(mnt, path, &parent_id, name)) {
        return SMKFS_ERR_INVAL;
    }

    return smkfs_delete_record(mnt, parent_id, name, record_id);
}