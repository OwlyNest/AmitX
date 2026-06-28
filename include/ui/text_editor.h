/*
	* include/ui/text_editor.h - [Enter description]
	* Author:   amity
	* Date:     Sun Jun 28 12:32:56 2026
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
#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#define EDITOR_MARGIN_X  8
#define EDITOR_MARGIN_Y  28
#define EDITOR_STATUS_H  20
/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
typedef struct {
	char *data;           /* File contents, null-terminated */
    uint32_t size;        /* Current size in bytes */
    uint32_t capacity;    /* Allocated size */
    uint32_t cursor;      /* Byte offset into data */
    uint32_t scroll_y;    /* First visible line (0-based) */
    int cx, cy;           /* Cursor position on screen (chars) */
    int cols, rows;       /* Text area dimensions (chars) */
    char path[128];       /* Filename */
    int dirty;            /* 1 if modified since last save */
} editor_t;
/* --- Globals ---*/

/* --- Prototypes ---*/
void amity_run(const char *path);          /* Main loop */

#endif