/*
	* amitx_types.h - [Enter description]
	* Author:   amity
	* Date:     Tue Jun  9 19:38:48 2026
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
#ifndef AMITX_TYPES_H
#define AMITX_TYPES_H
/* --- Macros ---*/

/* --- Includes ---*/
#include <stdint.h>
#include <stddef.h>
/* --- Typedefs - Structs - Enums ---*/
typedef void (*irq_handler_t)(void);
typedef void (*isr_handler_t)(uint32_t int_no, uint32_t err);
typedef void (*task_func_t)(void);
typedef int  (*syscall_func_t)(uint32_t a1, uint32_t a2, uint32_t a3);
/* --- Globals ---*/

/* --- Prototypes ---*/
#endif