/*
	* exec/loader.c - [Enter description]
	* Author:   amity
	* Date:     Mon Jun 29 12:57:18 2026
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
#include <exec/loader.h>
#include <exec/amx.h>
#include <fs/amfs.h>
#include <lib/string.h>
#include <screen/printk.h>
#include <mm/heap.h> /* mem... functions live in heap.c, move to string.c later */
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/
/* ==========================================================================
 *                                                                          *
 * exec_load()                                                              *
 *                                                                          *
 * Reads AMX file from disk                                                 *
 * Parses and validates header                                              *
 * Fills context                                                            *
 * Returns 0 on success.                                                    *
 *                                                                          *
 ========================================================================== */
int exec_load(const char *path, exec_context_t *ctx) {
	/* Validate path */
	if (!path || path[0] != '/') return -1;       /* Give a path at least */
    if (strcmp(path, "/") == 0) return -1; /* You can't create root */
    if (!amfs_exists(path)) return -1;            /* File doesn't exist yet */

	/* Read just the header first */
    uint8_t hdr_buf[AMX_HEADER_SIZE];
    int len = amfs_read(path, (char *)hdr_buf, sizeof(hdr_buf));
    if (len < AMX_HEADER_SIZE) return -1;

    amx_header_t header;
    if (read_header(hdr_buf, &header) != 0) return -1;
    if (verify_header(&header) != AMX_OK) return -1;

    /* Now allocate the right size */
    uint32_t total = AMX_HEADER_SIZE + header.image_size;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return -1;

    /* Read the whole file */
    len = amfs_read(path, (char *)buf, total);
	for (int i = 92; i < 130; i++)
		printk("%02X ", buf[i]);
    if (len < (int)total) {
        free(buf);
        return -1;
    }

	/* Display header to user */
	print_header(&header);

	ctx->header     = header;
	ctx->file_image = buf;
	ctx->image_base = NULL;
	ctx->stack      = NULL;

	return 0;
}

/* ==========================================================================
 *                                                                          *
 * exec_map()                                                               *
 *                                                                          *
 * Converts file to memory layout                                           *
 * Allocates heap memory                                                    *
 * Copies image + sets up BSS/Stack                                         *
 * Returns 0 on success.                                                    *
 *                                                                          *
 ========================================================================== */
int exec_map(exec_context_t *ctx) {
	ctx->image_base = malloc(ctx->header.image_size + ctx->header.bss_size);
	if (!ctx->image_base) {
		return -1;
	}

	memcpy(ctx->image_base, ctx->file_image + AMX_HEADER_SIZE, ctx->header.image_size);
	printk("right after memcpy:\n");
	for (size_t i = 0; i < 49; i++) {

		printk("%02X ", ((uint8_t *)ctx->image_base)[i]);
	}
	printk("\nDone\n");

	ctx->stack = malloc(ctx->header.stack_size);
	if (!ctx->stack) {
		free(ctx->image_base);
		ctx->image_base = NULL;
		return -1;
	}
	printk("image_base = %p\n", ctx->image_base);
	printk("stack      = %p\n", ctx->stack);

	memset( (uint8_t *)ctx->image_base + ctx->header.image_size, 0, ctx->header.bss_size);
	return 0;
}

/* ==========================================================================
 *                                                                          *
 * exec_start()                                                             *
 *                                                                          *
 * Transfer control                                                         *
 * Set IP, switch stack, jump to ring 3 (userspace)                         *
 * Returns 0 on success.                                                    *
 *                                                                          *
 ========================================================================== */
int exec_start(exec_context_t *ctx) {
	if (!ctx || !ctx->image_base) return -1;

	/* Entry point is offset from image_base */
    void (*entry)(void) = (void (*)(void))((uint32_t)ctx->image_base + ctx->header.entry);

	printk("[AMX] Jumping to entry at 0x%x\n", ctx->header.entry);

	entry();

	printk("[AMX] Program returned\n");


	return 0;
}

void exec_cleanup(exec_context_t *ctx) {
    if (ctx->stack) { free(ctx->stack); ctx->stack = NULL; }
    if (ctx->image_base) { free(ctx->image_base); ctx->image_base = NULL; }
    if (ctx->file_image) { free(ctx->file_image); ctx->file_image = NULL; }
}