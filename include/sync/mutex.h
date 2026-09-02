/*
 * include/sync/mutex.h - [Enter description]
 * Author:   amity
 * Date:     Sun Jul  5 14:48:40 2026
 * Copyright © 2026 OwlyNest
 */

/* --- Styling Instructions ---
 * Encoding:                      UTF-8, Unix line endings
 * Text font:                     Monospace
 * Line width:                    Max 80 characters
 * Indentation:                   Use 4 spaces
 * Brace style:                   Same line as control statement
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

/* --- Macros ---*/
#ifndef __SYNC_MUTEX_H__
#define __SYNC_MUTEX_H__

#include <stdint.h>
#define MUTEX_NO_TIMEOUT 0
#define MUTEX_WAIT_FOREVER 0xFFFFFFff
/* --- Includes ---*/
#include <arch/x86/task.h>
#include <sync/spinlock.h>
/* --- Typedefs - Structs - Enums ---*/
typedef struct mutex {
  int locked;
  task_t *owner;
  BYTE owner_orig_prio; /* Saved priority before boost */
  _SPINLOCK lock;
  task_queue_t waiters;
} _MUTEX;
/* --- Globals ---*/

/* --- Prototypes ---*/
VOID mutex_init(_MUTEX *m);
INT mutex_trylock(_MUTEX *m);
INT mutex_lock(_MUTEX *m, ULONG timeout_ms);
VOID mutex_unlock(_MUTEX *m);
#endif