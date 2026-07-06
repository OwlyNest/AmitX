/*
	* sync/mutex.c - [Enter description]
	* Author:   amity
	* Date:     Sun Jul  5 14:48:48 2026
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
#include "arch/x86/task.h"
#include "sync/spinlock.h"
#include <sync/mutex.h>
#include <stddef.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

void mutex_init(mutex_t *m) {
	if (!m) return;

	m->locked = 0;
	m->owner = NULL;
	spinlock_init(&m->lock);
	task_queue_init(&m->waiters);
}

int mutex_trylock(mutex_t *m) {
	if (!m) return 0;

	uint32_t flags = spinlock_acquire(&m->lock);
	int aquired = 0;

	if (!m->locked) {
		m->locked = 1;
		m->owner = task_current();
		aquired = 1;
	}

	spinlock_release(&m->lock, flags);
	return aquired;
}

int mutex_lock(mutex_t *m, uint32_t timeout_ms) {
	if (!m) return 0;
	if (mutex_trylock(m)) return 1;

	for (;;) {
		uint32_t wait_ms = (timeout_ms == MUTEX_WAIT_FOREVER) ? TASK_NO_TIMEOUT : timeout_ms;
		wake_reason_t reason = task_block_on(&m->waiters, wait_ms);

		if (reason == WAKE_TIMEOUT) return 0;
		if (mutex_trylock(m)) return 1;
		if (timeout_ms != MUTEX_WAIT_FOREVER) return 0;
	}
}

void mutex_unlock(mutex_t *m) {
	if (!m) return;

	uint32_t flags = spinlock_acquire(&m->lock);
	m->locked = 0;
	m->owner = NULL;
	spinlock_release(&m->lock, flags);

	task_wake_one(&m->waiters);
}