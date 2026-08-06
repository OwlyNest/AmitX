/*
	* fs/smkfs/inode.c - Inode Attribute Operations
	* Author:   amity
	* Date:     Wed Jul 29 17:39:00 2026
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

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

SMKFS_STATUS smkfs_getattr(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, _SMKFS_RECORD *rec, PVOID attr_buf, SIZE_T buf_size) {
    LONG ret;
    if (!mnt->mounted || !rec || !attr_buf || record_id == 0) {
        return SMKFS_ERR_INVAL;
    }

    ret = record_read(mnt, record_id, rec, attr_buf, buf_size);
    return (ret < 0) ? (SMKFS_STATUS)ret : SMKFS_OK;
}

SMKFS_STATUS smkfs_setattr(_SMKFS_MOUNT *mnt, SMKFS_RECORD_ID record_id, SMKFS_ATTR_TYPE attr_type, PCVOID data, SIZE_T len) {
    UCHAR attr_buf[SMKFS_BLOCK_SIZE - sizeof(_SMKFS_RECORD)];
    _SMKFS_RECORD rec;

    if (!mnt->mounted || !data || record_id == 0) return SMKFS_ERR_INVAL;
    if (record_read(mnt, record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
    }

    if (record_add_attr(attr_buf, sizeof(attr_buf), attr_type, data, len) != SMKFS_OK) {
        return SMKFS_ERR_NOSPC;
    }

    rec.attr_count++;
    rec.header.length = sizeof(_SMKFS_RECORD) + attr_buf_total_len(attr_buf);
    return record_write(mnt, record_id, &rec, attr_buf);
}

SMKFS_STATUS smkfs_stat(_SMKFS_MOUNT *mnt, SMKFS_PATH path, _SMKFS_RECORD *rec, PVOID attr_buf, SIZE_T buf_size) {
    SMKFS_RECORD_ID record_id;

    if (!mnt->mounted || !path || !rec || !attr_buf || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    if (path_lookup(mnt, path, &record_id) != SMKFS_OK) {
        return SMKFS_ERR_NOTFOUND;
    }

    return smkfs_getattr(mnt, record_id, rec, attr_buf, buf_size);
}

SMKFS_STATUS smkfs_chmod(_SMKFS_MOUNT *mnt, SMKFS_PATH path, SMKFS_PERM permissions) {
    SMKFS_RECORD_ID record_id;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    if (path_lookup(mnt, path, &record_id) != SMKFS_OK) {
        return SMKFS_ERR_NOTFOUND;
    }

    return smkfs_setattr(mnt, record_id, SMKFS_ATTRT_PERMISSIONS, &permissions, sizeof(permissions));
}

SMKFS_STATUS smkfs_chown(_SMKFS_MOUNT *mnt, SMKFS_PATH path, ULONG uid, ULONG gid) {
    SMKFS_RECORD_ID record_id;
    ULONGLONG owner = ((ULONGLONG)uid << 32) | gid;

    if (!mnt->mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
    }

    if (path_lookup(mnt, path, &record_id) != SMKFS_OK) {
        return SMKFS_ERR_NOTFOUND;
    }

    return smkfs_setattr(mnt, record_id, SMKFS_ATTRT_OWNER, &owner, sizeof(owner));
}