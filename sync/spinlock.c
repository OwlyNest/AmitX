/*
	* sync/spinlock.c - [Enter description]
	* Author:   amity
	* Date:     Sun Jul  5 14:44:41 2026
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
#include <sync/spinlock.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

void spinlock_init(spinlock_t *lock) {
	(void)lock;
}

uint32_t spinlock_acquire(spinlock_t *lock) {
	uint32_t flags;

	(void)lock;

	asm volatile (
		"pushfl\n\t"
		"pop %0\n\t"
		"cli"
		: "=r" (flags)
		:
		: "memory"
	);

	return flags;
}

void spinlock_release(spinlock_t *lock, uint32_t flags) {
    (void)lock;

    asm volatile (
        "push %0\n\t"
        "popfl"
        :
        : "r" (flags)
        : "memory", "cc"
    );
}