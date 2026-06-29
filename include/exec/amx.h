/*
	* include/exec/amx.h - [Enter description]
	* Author:   amity
	* Date:     Mon Jun 29 09:58:18 2026
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
#ifndef AMX_H
#define AMX_H

#define AMX_VERSION          1

#define AMX_HEADER_SIZE      92
#define AMX_MAX_IMAGE_SIZE   (16 * 1024 * 1024)

#define AMX_OFF_MAGIC        0
#define AMX_OFF_VERSION      4
#define AMX_OFF_FLAGS        6
#define AMX_OFF_IMAGE_SIZE   8
#define AMX_OFF_ENTRY        12
#define AMX_OFF_BSS_SIZE     16
#define AMX_OFF_STACK_SIZE   20
#define AMX_OFF_PROGRAM_NAME 24
#define AMX_OFF_AUTHOR       56
#define AMX_OFF_CHECKSUM     88
/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
typedef struct amx_header {
	char magic[4];          // "AMX\0"

    uint16_t version;
    uint16_t flags;

    uint32_t image_size;
    uint32_t entry;

    uint32_t bss_size;

    uint32_t stack_size;

	char program_name[32];
	char author[32];

    uint32_t checksum;
} amx_header_t;

typedef enum {
    AMX_OK = 0,

    AMX_BAD_HEADER,
    AMX_BAD_MAGIC,
    AMX_BAD_VERSION,
    AMX_BAD_FLAGS,
    AMX_BAD_IMAGE_SIZE,
    AMX_BAD_ENTRY,
    AMX_BAD_BSS_SIZE,
    AMX_BAD_STACK_SIZE,
    AMX_BAD_PROGRAM_NAME,
    AMX_BAD_AUTHOR,
    AMX_BAD_CHECKSUM
} amx_status_t;
/* --- Globals ---*/

/* --- Prototypes ---*/
int read_header(const uint8_t *buf, amx_header_t *header);
int verify_header(amx_header_t *header);
void print_header(amx_header_t *header);
#endif