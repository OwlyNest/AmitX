/*
 * include/sync/spinlock.h - Spinlock interface
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
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers: Use 3 '-' characters before and after
 * Pointer notation:              Next to variable name, not type
 * Binary operations:             Space around operator
 * Empty parameter list:          Use (void) instead of ()
 * Statements and declarations:   Max one per line
 */

#ifndef __SYNC_SPINLOCK_H__
#define __SYNC_SPINLOCK_H__

/* --- Macros ---*/

#include "internal/phonon_types.h"
#define SPINLOCK_INITIALIZER {0}

/* --- Includes ---*/

#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

#if ARCH_X86_64

typedef ULONGLONG _SPINLOCK_FLAGS;

#else

typedef ULONG _SPINLOCK_FLAGS;

#endif

typedef struct spinlock {
  volatile LONG value;
} _SPINLOCK;

/* --- Globals ---*/

/* --- Prototypes ---*/

VOID spinlock_init(_SPINLOCK *lock);
_SPINLOCK_FLAGS spinlock_acquire(_SPINLOCK *lock);
VOID spinlock_release(_SPINLOCK *lock, _SPINLOCK_FLAGS flags);

#endif