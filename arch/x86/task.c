/*
	* arch/x86/task.c - [Enter description]
	* Author:   amity
	* Date:     Sat Jul  4 01:33:01 2026
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
#include <stddef.h>
#include <mm/heap.h>
#include <lib/string.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static task_t *current_task = NULL;
static tid_t next_tid = 1;
/* --- Prototypes ---*/

/* --- Functions ---*/

void task_init(void) {
    current_task = NULL;
}

task_t *task_create(void (*entry)(void), const char *name) {
	(void)entry;

	task_t *task = malloc(sizeof(task_t));
	if (!task) return NULL;

	memset(task, 0, sizeof(task_t));

	/* Identity */
    task->tid = next_tid++;
    task->pid = task->tid;

    /* Scheduling */
	task->ring = 0;
	task->sched_class = SCHED_NORMAL;

	task->state = TASK_READY;

	task->quantum = 0;
	task->sleep_until = 0;

	if (name) {
		strncpy(task->name, name, TASK_NAME_LEN - 1);
	}

	/* Allocate kernel stack */
    task->kernel_stack_size = TASK_STACK_SIZE;
    task->kernel_stack = malloc(TASK_STACK_SIZE);

    if (!task->kernel_stack) {
        free(task);
        return NULL;
    }

    /* Build initial stack */
    uint32_t *stack =
        (uint32_t *)((uint8_t *)task->kernel_stack + TASK_STACK_SIZE);

    /*
     * Stack grows downward.
     *
     * Layout expected by context_switch():
     *
     *      EDI
     *      ESI
     *      EBX
     *      EBP
     *      RET -> entry()
     */

    *--stack = (uint32_t)entry;   /* ret */
    *--stack = 0;                 /* ebp */
    *--stack = 0;                 /* ebx */
    *--stack = 0;                 /* esi */
    *--stack = 0;                 /* edi */

    task->esp = (uint32_t)stack;

	return task;
}

void task_destroy(task_t *task) {
	if (!task) {
		return;
	}
	free(task);
}

task_t *task_current(void) {
    return current_task;
}

void task_set_current(task_t *task) {
    current_task = task;
}