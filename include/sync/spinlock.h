/*
	* include/sync/spinlock.h - [Enter description]
	* Author:   amity
	* Date:     Sun Jul  5 14:44:52 2026
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
#ifndef __SPINLOCK__
#define __SPINLOCK__
/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
/*
 * Single-CPU only. There's no other core to spin against, so the
 * only hazard is the local timer IRQ preempting mid-update. If
 * OwlyNest grows SMP, this needs a real test-and-set loop too.
 */
typedef struct spinlock {
    int unused;
} spinlock_t;


/* --- Globals ---*/

/* --- Prototypes ---*/
void spinlock_init(spinlock_t *lock);
uint32_t spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock, uint32_t flags);
#endif