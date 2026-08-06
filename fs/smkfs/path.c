/*
	* fs/smkfs/path.c - Path Resolution (G1)
	* Author:   amity
	* Date:     Wed Jul 29 17:39:04 2026
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

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

SMKFS_STATUS path_lookup(_SMKFS_MOUNT *mnt, SMKFS_PATH path, SMKFS_RECORD_ID *out_record) {
    PCCHAR p;
    CHAR name[SMKFS_NAME_LEN];
    SMKFS_RECORD_ID current;
    LONG i;
    UCHAR path_drive;

    if (!path || path[1] != ':' || path[2] != '/') return SMKFS_ERR_INVAL;

    path_drive = path[0] - 'A';
    if (path_drive != mnt->drive_num) return SMKFS_ERR_INVAL;

    current = mnt->sb.root_record_id;
    p = path + 3;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        i = 0;
        while (*p && *p != '/' && i < SMKFS_NAME_LEN - 1) {
            name[i++] = *p++;
        }
        
        name[i] = '\0';

        if (smkfs_lookup_by_name(mnt, current, name, &current) != SMKFS_OK) {
            return SMKFS_ERR_NOTFOUND;
        }
    }

    if (out_record) *out_record = current;
    return SMKFS_OK;
}

SMKFS_STATUS path_split(_SMKFS_MOUNT *mnt, SMKFS_PATH path, SMKFS_RECORD_ID *out_parent, PCHAR out_name) {
    PCCHAR last_slash = strrchr(path, '/');
    if (!last_slash || last_slash == path + 2) {
        *out_parent = mnt->sb.root_record_id;
        LONG len = strlen(path + 3);
        if (len >= SMKFS_NAME_LEN) return SMKFS_ERR_TOO_BIG;
        memcpy(out_name, path + 3, len + 1);
    } else {
        CHAR parent_path[SMKFS_NAME_LEN];
        LONG len = last_slash - path;
        if (len >= SMKFS_NAME_LEN) return SMKFS_ERR_TOO_BIG;
        memcpy(parent_path, path, len);
        parent_path[len] = '\0';
        if (path_lookup(mnt, parent_path, out_parent) != SMKFS_OK) {
            return SMKFS_ERR_NOTFOUND;
        }

        LONG name_len = strlen(last_slash + 1);
        if (name_len >= SMKFS_NAME_LEN) return SMKFS_ERR_TOO_BIG;
        memcpy(out_name, last_slash + 1, name_len + 1);
    }
    return SMKFS_OK;
}