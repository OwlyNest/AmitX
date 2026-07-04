/*
	* arch/x86/scheduler.c - [Enter description]
	* Author:   amity
	* Date:     Sat Jul  4 13:00:37 2026
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
#include <arch/x86/scheduler.h>
#include <arch/x86/task.h>
#include <stddef.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
/* --- Includes ---*/

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static task_t *ready_list = NULL;
static task_t *current = NULL;
/* --- Prototypes ---*/

/* --- Functions ---*/

static int scheduler_init(void) {
	ready_list = 0;
	current = 0;
	return 0;
}

kscope_node_t scheduler_node = {
	.name = "x86-scheduler",
	.id = 0x0011,
	.class = KSCOPE_CLASS_TIME,
	.subclass = KSCOPE_SUBCLASS_TIME_SCHED,
	.requires = (kscope_node_t *[]){ &pit_timer_node, &x86_gdt_node },
    .require_count = 2,
    .provides = (const char *[]){"sched."},
    .provide_count = 1,
    .init = scheduler_init,
};


void scheduler_add(task_t *task) {
	if (!task) return;

	task->state = TASK_READY;

	if (!ready_list) {
		ready_list = task;
		task->next = task;
		task->prev = task;
		return;
	}

	task->next = ready_list;
	task->prev = ready_list->prev;
	ready_list->prev->next = task;
	ready_list->prev = task;

}

void scheduler_remove(task_t *task) {
	if (!task || !ready_list) {
		return;
	}

	if (task->next == task) {
		ready_list = NULL;
		current = NULL;
	} else {
		task->prev->next = task->next; /* current = next*/
		task->next->prev = task->prev;

		if (ready_list == task) {
            ready_list = task->next;
		}

        if (current == task) {
            current = task->next;
		}
	}

	task->next = NULL;
    task->prev = NULL;
}

task_t *scheduler_next(void) {
    if(!current) {
        current = ready_list;
    } else {
        current = current->next;
    }
	return current;
}

void scheduler_tick(void) {
	if (!current) {
		return;
	}
	if (current->quantum) {
		current->quantum--;
	}
}