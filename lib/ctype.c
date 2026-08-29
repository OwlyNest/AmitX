/*
	* lib/ctype.c - [Enter description]
	* Author:   amity
	* Date:     Sun Jul  5 16:27:13 2026
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

/* --- Includes ---*/
#include <lib/ctype.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

int isdigit(int c) {
	return c >= '0' && c <= '9';
}

int isxdigit(int c) {
	return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isspace(int c) {
	return c==' '||c=='\n'||c=='\t'||c=='\r'||c=='\f'||c=='\v';
}

int isprint(int c) {
	return c >= 32 && c <= 126;
}

int toupper(int c) {
	return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

int tolower(int c) {
	return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}