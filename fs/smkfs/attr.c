/*
	* fs/smkfs/attr.c - Central Attribute Organ
	* Author:   amity
	* Date:     Wed Jul 29 17:38:31 2026
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
#include <screen/printk.h>
#include <lib/string.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

/*
	* The central attribute organ is the single source of truth for how every
	* attribute type behaves.  Adding a new attribute to SmKFS means
	* adding one row to the attr_registry[] table below -- nothing more.
*/

/* --- Attribute Validation Helpers --- */

static int attr_validate_u16(const void *data, size_t len) {
	(void)data;
    return (len == 2) ? SMKFS_OK : SMKFS_ERR_INVAL;
}

static int attr_validate_u64(const void *data, size_t len) {
	(void)data;
    return (len == 8) ? SMKFS_OK : SMKFS_ERR_INVAL;
}

static int attr_validate_name(const void *data, size_t len) {
    if (len == 0 || len > SMKFS_NAME_LEN) return SMKFS_ERR_INVAL;
    if (((const char *)data)[len - 1] != '\0') return SMKFS_ERR_INVAL;
    return SMKFS_OK;
}

static int attr_validate_extents(const void *data, size_t len) {
	(void)data;
    return (len % sizeof(smkfs_extent_t) == 0) ? SMKFS_OK : SMKFS_ERR_INVAL;
}

/* --- Attribute Debug Print Helpers --- */

static void attr_print_u16(const void *data, size_t len) {
    if (len == 2) printk("%u", *(const uint16_t *)data);
    else printk("<bad u16>");
}

static void attr_print_u64(const void *data, size_t len) {
    if (len == 8) printk("%llu", *(const uint64_t *)data);
    else printk("<bad u64>");
}

static void attr_print_name(const void *data, size_t len) {
	(void)len;
    printk("'%s'", (const char *)data);
}

static void attr_print_string(const void *data, size_t len) {
	(void)len;
    printk("\"%s\"", (const char *)data);
}

static void attr_print_extents(const void *data, size_t len) {
    uint32_t n = len / sizeof(smkfs_extent_t);
    const smkfs_extent_t *e = (const smkfs_extent_t *)data;
    printk("[%u extents]", n);
    for (uint32_t i = 0; i < n && i < 3; i++) {
        printk(" {log=%llu phys=%llu cnt=%u}",
                e[i].logical_offset, e[i].physical_block, e[i].block_count);
    }
    if (n > 3) printk(" ...");
}

/* --- The Central Attribute Registry --- */

static const smkfs_attr_def_t attr_registry[] = {
    {
	  SMKFS_ATTRT_END,
	  "END",
      SMKFS_ATTRF_UNIQUE | SMKFS_ATTRF_REQUIRED,
	  0,
      NULL,
	  NULL
	},

    {
	  SMKFS_ATTRT_NAME,
	  "NAME",
      SMKFS_ATTRF_UNIQUE,
	  0,
      attr_validate_name,
	  attr_print_name
	},

    {
	  SMKFS_ATTRT_DATA,
	  "DATA",
      SMKFS_ATTRF_UNIQUE,
	  8,
      attr_validate_u64,
	  attr_print_u64
	},

    { 
	  SMKFS_ATTRT_FSIZE,
	  "FSIZE",
      SMKFS_ATTRF_UNIQUE,
	  8,
      attr_validate_u64,
	  attr_print_u64
	},

    {
	  SMKFS_ATTRT_PERMISSIONS,
	  "PERMISSIONS",
      SMKFS_ATTRF_UNIQUE,
	  2,
      attr_validate_u16,
	  attr_print_u16
	},

    { 
	  SMKFS_ATTRT_OWNER,
	  "OWNER",
      SMKFS_ATTRF_UNIQUE,
	  8,
      attr_validate_u64,
	  attr_print_u64
	},

    {
	  SMKFS_ATTRT_TIMESTAMPS,
	  "TIMESTAMPS",
      SMKFS_ATTRF_UNIQUE,
	  0,
      NULL,
	  NULL
	},

    {
	  SMKFS_ATTRT_PARENT,
	  "PARENT",
      SMKFS_ATTRF_UNIQUE,
	  8,
      attr_validate_u64,
	  attr_print_u64
	},

    {
	  SMKFS_ATTRT_EXTENTS,
	  "EXTENTS",
      0,
	  0,
      attr_validate_extents,
	  attr_print_extents
	},

    {
	  SMKFS_ATTRT_SYMLINK,
	  "SYMLINK",
      SMKFS_ATTRF_UNIQUE,
	  0,
      NULL,
	  attr_print_string
	},

    {
	  SMKFS_ATTRT_DEVICE,
	  "DEVICE",
      SMKFS_ATTRF_UNIQUE,
	  8,
      attr_validate_u64,
	  attr_print_u64
	},

    {
	  0,
	  NULL,
	  0,
	  0,
	  NULL,
	  NULL
	}
};

/* --- Registry Accessors --- */

const smkfs_attr_def_t *smkfs_attr_lookup(uint16_t type) {
    for (const smkfs_attr_def_t *p = attr_registry; p->name != NULL; p++) {
        if (p->type == type) return p;
    }
    return NULL;
}

const char *smkfs_attr_name(uint16_t type) {
    const smkfs_attr_def_t *def = smkfs_attr_lookup(type);
    return (def != NULL) ? def->name : "UNKNOWN";
}

void smkfs_attr_debug_print(uint16_t type, const void *data, size_t len) {
    const smkfs_attr_def_t *def = smkfs_attr_lookup(type);
    if (def && def->debug_print) {
        def->debug_print(data, len);
    } else {
        printk("<%zu bytes>", len);
    }
}

/* --- Attribute Buffer Operations --- */

int record_find_attr(const void *attr_buf, uint16_t attr_type, void **out_attr, size_t *out_len) {
    const uint8_t *ptr = (const uint8_t *)attr_buf;

    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        if (ah->type == SMKFS_ATTRT_END) break;
        if (ah->type == attr_type) {
            if (out_attr) *out_attr = (void *)(ptr + sizeof(smkfs_attr_header_t));
            if (out_len) *out_len = ah->length;
            return SMKFS_OK;
        }
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }
    return SMKFS_ERR_NOTFOUND;
}

int record_add_attr(void *attr_buf, size_t buf_size, uint16_t attr_type, const void *data, size_t data_len) {
    uint8_t *ptr = (uint8_t *)attr_buf;
    size_t used = 0;

    if (!attr_buf || (data_len > 0 && !data)) {
        return SMKFS_ERR_INVAL;
	}

    if (buf_size < 2 * sizeof(smkfs_attr_header_t) + data_len) {
        return SMKFS_ERR_NOSPC;
	}

    record_remove_attr(attr_buf, attr_type);

    ptr = (uint8_t *)attr_buf;
    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        if (ah->type == SMKFS_ATTRT_END) {
            used = (size_t)(ptr - (uint8_t *)attr_buf) + sizeof(smkfs_attr_header_t);
            break;
        }
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }

    size_t need = sizeof(smkfs_attr_header_t) + data_len + sizeof(smkfs_attr_header_t);
    if (used + need > buf_size) return SMKFS_ERR_NOSPC;

    smkfs_attr_header_t *new_ah = (smkfs_attr_header_t *)(ptr);
    new_ah->type = attr_type;
    new_ah->flags = 0;
    new_ah->id = 0;
    new_ah->length = (uint32_t)data_len;
    memcpy(ptr + sizeof(smkfs_attr_header_t), data, data_len);

    smkfs_attr_header_t *term = (smkfs_attr_header_t *)(ptr + sizeof(smkfs_attr_header_t) + data_len);
    term->type = SMKFS_ATTRT_END;
    term->flags = 0;
    term->id = 0;
    term->length = 0;

    return SMKFS_OK;
}

int record_remove_attr(void *attr_buf, uint16_t attr_type) {
    uint8_t *ptr = (uint8_t *)attr_buf;
    uint8_t *found = NULL;
    size_t found_len = 0;

    while (1) {
        smkfs_attr_header_t *ah = (smkfs_attr_header_t *)ptr;
        if (ah->type == SMKFS_ATTRT_END) break;
        if (ah->type == attr_type) {
            found = ptr;
            found_len = sizeof(smkfs_attr_header_t) + ah->length;
        }
        ptr += sizeof(smkfs_attr_header_t) + ah->length;
    }

    if (!found) return SMKFS_OK;

    size_t tail = (size_t)(ptr - (found + found_len));
    memmove(found, found + found_len, tail);
    return SMKFS_OK;
}