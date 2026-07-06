/*
	* include/lib/ctype.h - [Enter description]
	* Author:   amity
	* Date:     Sun Jul  5 16:27:20 2026
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
#ifndef __CTYPE__
#define __CTYPE__
/* --- Includes ---*/

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
int isdigit(int c);
int isxdigit(int c);
int isspace(int c);
int isprint(int c);
int toupper(int c);
int tolower(int c);
#endif