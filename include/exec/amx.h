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
#ifndef __EXEC_AMX_H__
#define __EXEC_AMX_H__

#define AMX_VERSION          1


#define AMX_MAX_IMAGE_SIZE   (16 * 1024 * 1024)

#define AMX_OFF_MAGIC        0x00
#define AMX_OFF_VERSION      0x04
#define AMX_OFF_FLAGS        0x06
#define AMX_OFF_IMAGE_OFF    0x08
#define AMX_OFF_IMAGE_SIZE   0x0C
#define AMX_OFF_ENTRY        0x10
#define AMX_OFF_BSS_SIZE     0x14
#define AMX_OFF_STACK_SIZE   0x18
#define AMX_OFF_RELOC_TBL    0x1C
#define AMX_OFF_RELOC_NUM    0x20
#define AMX_OFF_PROGRAM_NAME 0x24
#define AMX_OFF_AUTHOR       0x44
#define AMX_OFF_CHECKSUM     0x64

#define AMX_RELOC_ABS32   0
/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* ========================================================================== * 
 *                                                                            *
 * +-----------------------+                                                  *
 * | Header                |                                                  *
 * +-----------------------+                                                  *
 * | Image                 |                                                  *
 * +-----------------------+                                                  *
 * | Relocation table      |                                                  *
 * +-----------------------+                                                  *
 *                                                                            *
 * ========================================================================== */
typedef struct {
    char     magic[4];          /* "AMX\0"                    */
    uint16_t version;           /* AMX_VERSION                */
    uint16_t flags;

    uint32_t image_offset;
    uint32_t image_size;        /* code + data               */

    uint32_t entry;             /* offset into image         */

    uint32_t bss_size;
    uint32_t stack_size;

    uint32_t reloc_offset;      /* from file start           */
    uint32_t reloc_count;

    char     program_name[32];
    char     author[32];

    uint32_t checksum;
} amx_header_t;
#define AMX_HEADER_SIZE      (int)sizeof(amx_header_t)
#define AMX_HEADER_SIZE_I    104

typedef struct {
    uint32_t offset;
    uint16_t type;
    uint16_t reserved;
} amx_reloc_t;

typedef enum {
    AMX_OK = 0,

    AMX_BAD_HEADER,
    AMX_BAD_MAGIC,
    AMX_BAD_VERSION,
    AMX_BAD_FLAGS,
    AMX_BAD_IMAGE_OFFSET,
    AMX_BAD_IMAGE_SIZE,
    AMX_BAD_ENTRY,
    AMX_BAD_BSS_SIZE,
    AMX_BAD_STACK_SIZE,
    AMX_BAD_RELOC_TBL,
    AMX_BAD_RELOC_NUM,
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