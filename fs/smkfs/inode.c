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

int smkfs_getattr(uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size) {
    if (!mounted || !rec || !attr_buf || record_id == 0) {
        return SMKFS_ERR_INVAL;
	}

    return record_read(record_id, rec, attr_buf, buf_size);
}

int smkfs_setattr(uint64_t record_id, uint16_t attr_type, const void *data, size_t len) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;

    if (!mounted || !data || record_id == 0) return SMKFS_ERR_INVAL;
    if (record_read(record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
	}

    if (record_add_attr(attr_buf, sizeof(attr_buf), attr_type, data, len) != 0) {
        return SMKFS_ERR_NOSPC;
	}

    rec.attr_count++;
    rec.header.length = sizeof(smkfs_record_t) + attr_buf_total_len(attr_buf);
    return record_write(record_id, &rec, attr_buf);
}

int smkfs_stat(const char *path, smkfs_record_t *rec, void *attr_buf, size_t buf_size) {
    uint64_t record_id;

    if (!mounted || !path || !rec || !attr_buf || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
	}

    if (path_lookup(path, &record_id) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    return smkfs_getattr(record_id, rec, attr_buf, buf_size);
}

int smkfs_chmod(const char *path, uint16_t permissions) {
    uint64_t record_id;

    if (!mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
	}

    if (path_lookup(path, &record_id) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    return smkfs_setattr(record_id, SMKFS_ATTRT_PERMISSIONS, &permissions, sizeof(permissions));
}

int smkfs_chown(const char *path, uint32_t uid, uint32_t gid) {
    uint64_t record_id;
    uint64_t owner = ((uint64_t)uid << 32) | gid;

    if (!mounted || !path || path[1] != ':' || path[2] != '/') {
        return SMKFS_ERR_INVAL;
	}

    if (path_lookup(path, &record_id) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    return smkfs_setattr(record_id, SMKFS_ATTRT_OWNER, &owner, sizeof(owner));
}