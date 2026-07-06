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
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/
#ifndef __SEMAPHORE__
#define __SEMAPHORE__

#define SEM_NO_TIMEOUT 0
#define SEM_WAIT_FOREVER 0xFFFFFFFF
/* --- Includes ---*/
#include <arch/x86/task.h>
#include <sync/spinlock.h>
/* --- Typedefs - Structs - Enums ---*/
typedef struct semaphore {
	uint32_t units;
	uint32_t max_units;
	spinlock_t lock;
	task_queue_t waiters;
} semaphore_t;
/* --- Globals ---*/

/* --- Prototypes ---*/
void sem_init(semaphore_t *sem, uint32_t initial_units, uint32_t max_units);
int sem_trywait(semaphore_t *sem, uint32_t units);
int sem_wait(semaphore_t *sem, uint32_t units, uint32_t timeout_ms);
void sem_signal(semaphore_t *sem, uint32_t units);
#endif