/*
	* include/drivers/font.h - [Enter description]
	* Author:   amity
	* Date:     Wed Jun 24 00:49:03 2026
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
#ifndef FONT_H
#define FONT_H
/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
extern const uint8_t font8x8[256][8];   // 8x8 font
/* --- Globals ---*/

/* --- Prototypes ---*/
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t color);
void fb_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color);
int  fb_get_string_width(const char* str);
#endif