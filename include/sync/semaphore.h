/*
 * include/sync/semaphore.h - [Enter description]
 * Author:   amity
 * Date:     Sun Jul  5 14:59:52 2026
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
#ifndef __SYNC_SEMAPHORE_H__
#define __SYNC_SEMAPHORE_H__

#define SEM_NO_TIMEOUT 0
#define SEM_WAIT_FOREVER 0xFFFFFFFF
/* --- Includes ---*/
#include <arch/x86/task.h>
#include <sync/spinlock.h>
/* --- Typedefs - Structs - Enums ---*/
typedef struct semaphore {
  ULONG units;
  ULONG max_units;
  _SPINLOCK lock;
  task_queue_t waiters;
} _SEMAPHORE;
/* --- Globals ---*/

/* --- Prototypes ---*/
VOID sem_init(_SEMAPHORE *sem, ULONG initial_units, ULONG max_units);
INT sem_trywait(_SEMAPHORE *sem, ULONG units);
INT sem_wait(_SEMAPHORE *sem, ULONG units, ULONG timeout_ms);
VOID sem_signal(_SEMAPHORE *sem, ULONG units);
#endif