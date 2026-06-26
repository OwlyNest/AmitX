/*
	* include/drivers/gfx_term.h - [Enter description]
	* Author:   amity
	* Date:     Fri Jun 26 09:42:04 2026
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
#ifndef GFX_TERM_H
#define GFX_TERM_H

#define TERM_FG_DEFAULT 0xFFFFFFFFu
#define TERM_BG_DEFAULT 0xFF282838u  /* dark panel */
/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
void gfx_term_init(int x, int y, int w, int h, uint32_t fg, uint32_t bg);
void gfx_term_clear(void);
void gfx_term_putc(char c);
void gfx_term_puts(const char *str);
void gfx_term_newline(void);
void gfx_term_putint(int num);
void gfx_term_puthex(uint32_t n);
void gfx_term_move_cursor(int col, int row);
void gfx_term_draw_prompt(void);
void gfx_term_backspace(void);

/* Convenience: clear + draw border + title */
void gfx_term_draw_frame(const char *title);
#endif