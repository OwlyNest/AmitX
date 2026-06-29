/*
	* exec/amx.c - [Enter description]
	* Author:   amity
	* Date:     Mon Jun 29 09:58:12 2026
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
#include <exec/amx.h>
#include <fs/amfs.h>
#include <lib/string.h>
#include <screen/printk.h>
#include <mm/heap.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/
static uint16_t get_u16(const uint8_t *buf, size_t offset) {
    return (uint16_t)buf[offset]
         | ((uint16_t)buf[offset + 1] << 8);
}

static uint32_t get_u32(const uint8_t *buf, size_t offset) {
    return (uint32_t)buf[offset]
         | ((uint32_t)buf[offset + 1] << 8)
         | ((uint32_t)buf[offset + 2] << 16)
         | ((uint32_t)buf[offset + 3] << 24);
}


/* ==========================================================================
 *                                                                          *
 * read_header()                                                            *
 *                                                                          *
 * Reads the AMX header, buffer sent by exec_load()                         *
 * Does not validate the header                                             *
 * Returns 0 on success                                                     *
 *                                                                          *
 * header layout: [offset-size]                                             *
 * Offset  Size  Field                                                      *
 * 0       4     Magic: "AMX\x01"                                           *
 * 4       2     Version                                                    *
 * 6       2     Flags                                                      *
 * 8       4     Image size                                                 *
 * 12      4     Entry point (offset from start of code)                    *
 * 16      4     BSS size (zero-initialized)                                *
 * 20      4     Stack size                                                 *
 * 24      32    Program name                                               *
 * 56      32    Author                                                     *
 * 88      4     Checksum                                                   *
 *                                                                          *
 * ======================================================================== */
int read_header(const uint8_t *buf, amx_header_t *header) {
	/* Parse fields */
	memcpy(header->magic, (buf + AMX_OFF_MAGIC), 4);
	header->version    = get_u16(buf, AMX_OFF_VERSION);
	header->flags      = get_u16(buf, AMX_OFF_FLAGS);
	header->image_size = get_u32(buf, AMX_OFF_IMAGE_SIZE);
	header->entry      = get_u32(buf, AMX_OFF_ENTRY);
	header->bss_size   = get_u32(buf, AMX_OFF_BSS_SIZE);
	header->stack_size = get_u32(buf, AMX_OFF_STACK_SIZE);
	memcpy(header->program_name,(buf + AMX_OFF_PROGRAM_NAME), 32);
	memcpy(header->author, (buf + AMX_OFF_AUTHOR), 32);
	header->checksum   = get_u32(buf, AMX_OFF_CHECKSUM);
	return 0;
}

int verify_header(amx_header_t *header) {
	if (!header) {
		return AMX_BAD_HEADER;
	}

	if (memcmp(header->magic, "AMX\0", 4)) {
		return AMX_BAD_MAGIC;
	}
	
	if (header->version != AMX_VERSION) {
		return AMX_BAD_VERSION;
	}

	/* change to:
	 * if (header->flags & ~AMX_SUPPORTED_FLAGS)
     *     return AMX_BAD_FLAGS;
	 * after adding flags
	 */
	if (header->flags != 0) {
		return AMX_BAD_FLAGS;
	}

	if (header->image_size == 0) {
		return AMX_BAD_IMAGE_SIZE;
	}

	if (header->entry >= header->image_size) {
		return AMX_BAD_ENTRY;
	}

	if (memchr(header->program_name, '\0', 32) == NULL) {
		return AMX_BAD_PROGRAM_NAME;
	}

	if (memchr(header->author, '\0', 32) == NULL) {
		return AMX_BAD_AUTHOR;
	}

	/* Verify checksum later */

	if (header->image_size > AMX_MAX_IMAGE_SIZE) {
		return AMX_BAD_IMAGE_SIZE;
	}

	return AMX_OK;
}

void print_header(amx_header_t *header) {
	if (!header) {
		printk("[AMX], no header\n");
	}

	printk("\n=== AMX header ===\n");
	printk("Magic:      %s\n", header->magic);
    printk("Version:    %u\n", header->version);
    printk("Flags:      0x%04x\n", header->flags);

    printk("Image size: %u bytes\n", header->image_size);
    printk("Entry:      0x%08x\n", header->entry);
    printk("BSS size:   %u bytes\n", header->bss_size);
    printk("Stack size: %u bytes\n", header->stack_size);

    printk("Program:    %s\n", header->program_name);
    printk("Author:     %s\n", header->author);

    printk("Checksum:   0x%08x\n", header->checksum);

    printk("===================\n");
}