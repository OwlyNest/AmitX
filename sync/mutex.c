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
#include <arch/x86/task.h>
#include <sync/spinlock.h>
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
	m->owner_orig_prio = 0;
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
		if (m->owner) {
			m->owner_orig_prio = m->owner->base_prio;
		}
		aquired = 1;
	}

	spinlock_release(&m->lock, flags);
	return aquired;
}

int mutex_lock(mutex_t *m, uint32_t timeout_ms) {
    if (!m) return 0;

    task_t *self = task_current();

    for (;;) {
        uint32_t flags = spinlock_acquire(&m->lock);

        if (!m->locked) {
            m->locked = 1;
            m->owner = self;
            if (self) m->owner_orig_prio = self->base_prio;
            spinlock_release(&m->lock, flags);
            return 1;
        }

        if (m->owner->cur_prio < self->cur_prio) {
            task_boost_priority(m->owner, self->cur_prio);
        }

        uint32_t wait_ms = (timeout_ms == MUTEX_WAIT_FOREVER) ? TASK_NO_TIMEOUT : timeout_ms;

        /* task_block_on pushes onto m->waiters and calls schedule()
         * while we're still holding m->lock (interrupts off). Nobody
         * can clear m->locked and check m->waiters until they also
         * acquire m->lock -- so unlock can never run in the window
         * between "we saw it locked" and "we're on the list".
        */
        wake_reason_t reason = task_block_on(&m->waiters, wait_ms);

        spinlock_release(&m->lock, flags);

        if (reason == WAKE_TIMEOUT) return 0;
        /* Signaled -- loop back, re-check !m->locked under the lock */
    }
}

void mutex_unlock(mutex_t *m) {
    if (!m) return;

    uint32_t flags = spinlock_acquire(&m->lock);

    if (m->owner && m->owner->cur_prio != m->owner_orig_prio) {
        task_unboost_priority(m->owner);
    }

    m->locked = 0;
    m->owner = NULL;
    m->owner_orig_prio = 0;

    /* Wake while still holding m->lock -- same reasoning as above,
     * mirrored from the other side.
    */
    task_wake_one(&m->waiters);

    spinlock_release(&m->lock, flags);
}