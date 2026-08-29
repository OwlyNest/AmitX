/*
	* include/exec/loader.h - [Enter description]
	* Author:   amity
	* Date:     Mon Jun 29 12:57:14 2026
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
#ifndef __EXEC_LOADER_H__
#define __EXEC_LOADER_H__
/* --- Includes ---*/
#include <exec/amx.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
typedef struct exec_context {

    amx_header_t header;
	uint8_t *file_image;

    void *image_base;
    void *stack;

} exec_context_t;
/* --- Globals ---*/

/* --- Prototypes ---*/
int exec_load(const char *path, exec_context_t *ctx);
int exec_map(exec_context_t *ctx);
int exec_relocate(exec_context_t *ctx);
int exec_start(exec_context_t *ctx);
void exec_cleanup(exec_context_t *ctx);
int exec_run(const char *path);
#endif