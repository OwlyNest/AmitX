/*
 * sync/semaphore.c - Counting semaphore with priority wake
 * Author:   amity
 * Date:     Sun Jul  5 14:59:47 2026
 * Copyright (c) 2026 OwlyNest
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

/* --- Includes ---*/
#include "arch/x86/task.h"
#include "sync/spinlock.h"
#include <sync/semaphore.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

void sem_init(_SEMAPHORE *sem, ULONG initial_units, ULONG max_units) {
  if (!sem)
    return;

  sem->units = initial_units;
  sem->max_units = max_units;
  spinlock_init(&sem->lock);
  task_queue_init(&sem->waiters);
}

int sem_trywait(_SEMAPHORE *sem, ULONG units) {
  if (!sem)
    return 0;

  ULONG flags = spinlock_acquire(&sem->lock);
  int acquired = 0;

  if (sem->units >= units) {
    sem->units -= units;
    acquired = 1;
  }

  spinlock_release(&sem->lock, flags);

  return acquired;
}

int sem_wait(_SEMAPHORE *sem, ULONG units, ULONG timeout_ms) {
  if (!sem)
    return 0;

  for (;;) {
    ULONG flags = spinlock_acquire(&sem->lock);

    if (sem->units >= units) {
      sem->units -= units;
      spinlock_release(&sem->lock, flags);
      return 1;
    }

    ULONG wait_ms =
        (timeout_ms == SEM_WAIT_FOREVER) ? SEM_NO_TIMEOUT : timeout_ms;

    /* task_block_on pushes onto sem->waiters and calls schedule()
     * while we still hold sem->lock -- sem_signal can't add units
     * and check waiters until it also takes sem->lock, so it can
     * never land in the gap between "not enough units" and "on
     * the list".
     */
    wake_reason_t reason = task_block_on(&sem->waiters, wait_ms);

    spinlock_release(&sem->lock, flags);

    if (reason == WAKE_TIMEOUT)
      return 0;
    /* Signaled -- loop back, re-check units under the lock */
  }
}

void sem_signal(_SEMAPHORE *sem, ULONG units) {
  if (!sem)
    return;

  ULONG flags = spinlock_acquire(&sem->lock);

  sem->units += units;
  if (sem->units > sem->max_units)
    sem->units = sem->max_units;

  /* Wake while units are available, still under the lock. Woken
   * tasks re-check units themselves in sem_wait -- some may lose
   * the race and just loop back to sleep, which is fine.
   */
  while (sem->units > 0 && sem->waiters.head) {
    task_wake_one(&sem->waiters);
  }

  spinlock_release(&sem->lock, flags);
}