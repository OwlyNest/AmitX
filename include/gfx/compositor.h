/*
	* include/gfx/compositor.h - Compositor
	* Author:   amity
	* Date:     Fri Jul  3 15:39:35 2026
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
#ifndef _COMPOSITOR_H
#define _COMPOSITOR_H
/* --- Includes ---*/
#include <stdint.h>
#include <gfx/window.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* ==========================================================================
 * Render control
 * ======================================================================= */
void compositor_init(void);
void compositor_render(void);
void compositor_request_update(void);
int compositor_needs_update(void);

/* ==========================================================================
 * Per-window compositing
 * ======================================================================= */
void compositor_blit_window(window_t *win);
#endif