/*
	* include/drivers/keyboard.h - Keyboard interface
	* Author:   amity
	* Date:     Sat Jun 20 22:53:20 2026
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

#ifndef __DRIVERS_KEYBOARD_H__
#define __DRIVERS_KEYBOARD_H__

/* --- Includes ---*/
#include <stdint.h>

/* --- Macros ---*/
/* Special non-ASCII key codes */
#define KEY_UP     0x80
#define KEY_DOWN   0x81
#define KEY_LEFT   0x82
#define KEY_RIGHT  0x83
#define KEY_ESC    0x1B

/* --- Prototypes ---*/
void reset_keyboard_state(void);

unsigned char keyboard_getchar(void);
int keyboard_poll(unsigned char *c);
int  keyboard_has_char(void);     /* non-blocking check */
void keyboard_flush(void);        /* clear buffer */

#endif