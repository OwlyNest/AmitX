/*
	* fs/smkfs/checksum.c - Checksum and Header Utilities
	* Author:   amity
	* Date:     Wed Jul 29 17:38:04 2026
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

void header_init(smkfs_header_t *h, uint16_t type, uint32_t length,
        uint32_t flags) {
    memcpy(h->magic, SMKFS_MAGIC, 4);
    h->version = SMKFS_VERSION;
    h->type = type;
    h->length = length;
    h->flags = flags;
    h->checksum = 0;
}

int header_validate(const smkfs_header_t *h, uint16_t expected_type) {
    if (memcmp(h->magic, SMKFS_MAGIC, 4) != 0) {
        printk("[SmKFS] Wrong magic, expected %.4s, got %.4s\n",
                SMKFS_MAGIC, h->magic);
        return SMKFS_ERR_CORRUPT;
    }
    if (h->version != SMKFS_VERSION) {
        printk("[SmKFS] Wrong version, expected %d, got %d\n",
                SMKFS_VERSION, h->version);
        return SMKFS_ERR_CORRUPT;
    }
    if (h->type != expected_type) {
        printk("[SmKFS] Wrong type, expected %d, got %d\n",
                expected_type, h->type);
        return SMKFS_ERR_CORRUPT;
    }
    return SMKFS_OK;
}

uint32_t checksum_compute(const void *data, size_t len) {
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t sum = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        sum ^= ptr[i];
        for (int j = 0; j < 8; j++) {
            if (sum & 1) {
                sum = (sum >> 1) ^ 0xEDB88320;
            } else {
                sum >>= 1;
            }
        }
    }
    return ~sum;
}

void header_checksum_update(smkfs_header_t *h, const void *data, size_t len) {
    h->checksum = 0;
    h->checksum = checksum_compute(data, len);
}

int header_checksum_verify(const smkfs_header_t *h, const void *data, size_t len) {
    uint32_t saved = h->checksum;
    uint8_t tmp_buf[SMKFS_BLOCK_SIZE];

    if (len > sizeof(tmp_buf)) {
        return SMKFS_ERR_CORRUPT;
    }

    memcpy(tmp_buf, data, len);
    ((smkfs_header_t *)tmp_buf)->checksum = 0;
    uint32_t computed = checksum_compute(tmp_buf, len);

    if (computed != saved) {
        return SMKFS_ERR_CORRUPT;
    }
	
    return SMKFS_OK;
}