/*
	* screen/printk.h - [Enter description]
	* Author:   amity
	* Date:     Thu Jun 11 10:01:54 2026
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
#ifndef __SCREEN_PRINTK_H__
#define __SCREEN_PRINTK_H__
/* --- Includes ---*/
#include <stdarg.h>
#include <stddef.h>
/* --- Typedefs - Structs - Enums ---*/
typedef enum {
	LEN_NONE,
	LEN_HH,
	LEN_H,
	LEN_L,
	LEN_LL,
	LEN_Z,
	LEN_T
} LengthModifier;

typedef struct {
    int left;
    int zero;
    int alternate;
    int plus;
    int space;

    int width;
    int precision;      // -1 means "not specified"

	LengthModifier length;    

    char specifier;
} FormatSpec;
/* --- Globals ---*/

/* --- Prototypes ---*/
int kvsnprintf(char *buf, size_t size, const char *fmt, va_list args);
int ksnprintf(char *buf, size_t size, const char *fmt, ...);
void printk(const char *fmt, ...);
void c3_test_printk(void);
#endif