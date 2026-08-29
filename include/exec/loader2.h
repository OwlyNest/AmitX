/*
	* include/exec/loader2.h - AMX v2 executable loader
	* Author:   amity
	* Date:     Mon Jul 20 10:42:19 2026
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
#ifndef __EXEC_LOADER2_H__
#define __EXEC_LOADER2_H__

/* --- Includes ---*/
#include <exec/amx2.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/*
 * Execution context for an AMX2 program.
 * Holds everything needed to map, relocate, and run.
 */
typedef struct exec2_context {
	amx2_header_t    header;
    amx2_section_t  *sections;      /* section_count entries, malloc'd */

    uint8_t         *file_image;    /* Entire file, malloc'd */
    uint32_t         file_size;

    void            *image_base;    /* Where we mapped in memory */
    void            *stack_base;    /* Stack allocation */

    /* Resolved import table: parallel to header.import_count */
    void           **import_table;  /* Array of function pointers */
} exec2_context_t;
/* --- Globals ---*/

/* --- Prototypes ---*/
int  exec2_load(const char *path, exec2_context_t *ctx);
int  exec2_map(exec2_context_t *ctx);
int  exec2_relocate(exec2_context_t *ctx);
int  exec2_resolve_imports(exec2_context_t *ctx);
int  exec2_start(exec2_context_t *ctx);
void exec2_cleanup(exec2_context_t *ctx);
int  exec2_run(const char *path);

#endif