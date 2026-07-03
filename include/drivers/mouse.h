/*
	* include/drivers/mouse.h - Mouse interface
	* Author:   amity
	* Date:     Sat Jun 20 22:57:12 2026
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

#ifndef MOUSE_H
#define MOUSE_H

/* --- Includes ---*/
#include <stdint.h>

extern volatile int mouse_x;
extern volatile int mouse_y;

/* --- Prototypes ---*/
void get_mouse_position(int *x, int *y);
void reset_mouse_position(void);
void mouse_refresh_cursor(void);

int mouse_button_state(void);
int mouse_left_down(void);
int mouse_left_pressed(void);
int mouse_left_released(void);

int mouse_right_down(void);

#endif