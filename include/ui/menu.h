/*
	* ui/menu.h - [Enter description]
	* Author:   amity
	* Date:     Wed Jun 10 12:20:50 2026
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
#ifndef __UI_MENU_H__
#define __UI_MENU_H__
/* --- Includes ---*/

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern int POINTER;
extern int load_cyclone;
extern int menu;
extern const char* main_menu[];
extern const int main_menu_count;

/* --- Prototypes ---*/
void menu_run(void);
void menu_select(int choice);
/* --- Main ---*/

/* --- Functions ---*/

#endif