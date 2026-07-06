/*
	* arch/x86/scheduler.c - Round-robin scheduler
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

/* --- Includes ---*/
#include <arch/x86/scheduler.h>
#include <arch/x86/task.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <screen/printk.h>
#include <stddef.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static task_t *ready_list = NULL;
static task_t *current = NULL;

extern void context_switch(task_t *old, task_t *new);
extern volatile uint32_t tick_count;

/* --- Prototypes ---*/

/* --- Functions ---*/

/* ==========================================================================
 * Add task to ready list (circular doubly-linked)
 * ======================================================================= */
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

/* ==========================================================================
 * Remove task from ready list
 * ======================================================================= */
void scheduler_remove(task_t *task) {
    if (!task || !ready_list) return;

    if (task->next == task) {
        /* Only task in list */
        ready_list = NULL;
    } else {
        task->prev->next = task->next;
        task->next->prev = task->prev;

        if (ready_list == task) {
            ready_list = task->next;
        }
    }

    task->next = NULL;
    task->prev = NULL;
}

/* ==========================================================================
 * Pick next ready task (round-robin)
 * ======================================================================= */
task_t *scheduler_next(void) {
    if (!ready_list) return NULL;

    task_t *start = current ? current : ready_list;
    task_t *t = start->next;

    /* Search forward from current for next READY task */
    while (t != start) {
        if (t->state == TASK_READY) {
            return t;
        }
        t = t->next;
    }

    /* Check current/start if it's still ready */
    if (start->state == TASK_READY || start->state == TASK_RUNNING) {
        return start;
    }

    /* No ready tasks found — return idle (always at head) */
    return ready_list;
}

/* ==========================================================================
 * Timer tick handler — called from PIT ISR
 * ======================================================================= */
void scheduler_tick(void) {
    /* Wake sleeping tasks */
    if (ready_list) {
		task_t *t = ready_list;
		do {
			if (t->state == TASK_SLEEPING && t->sleep_until <= tick_count) {
				t->state = TASK_READY;
				t->sleep_until = 0;
			} else if (t->state == TASK_BLOCKED && t->sleep_until != 0 && t->sleep_until <= tick_count) {
				if (t->wait_queue) {
					task_queue_remove(t->wait_queue, t);
				}
				t->wake_reason = WAKE_TIMEOUT;
				t->sleep_until = 0;
				t->state = TASK_READY;
			}
			t = t->next;
		} while (t != ready_list);
	}

    /* Decrement current task quantum */
    task_t *task = task_current();
    if (!task) return;

    if (task->quantum > 0) {
        task->quantum--;
    }

    if (task->quantum == 0) {
        task->quantum = TASK_DEFAULT_QUANTUM;
        schedule();
    }
}

/* ==========================================================================
 * Schedule — switch to next ready task
 * ======================================================================= */
void schedule(void) {
    task_t *old = task_current();
    task_t *next = scheduler_next();

    if (!next) return;
    if (old == next) return;

    if (old && old->state == TASK_RUNNING) {
        old->state = TASK_READY;
    }

    next->state = TASK_RUNNING;
    task_set_current(next);

    context_switch(old, next);
}

/* ==========================================================================
 * Test tasks (enable for scheduler verification)
 * ======================================================================= */
#if 0
static void test_task_a(void) {
    for (int i = 0; i < 5; i++) {
        printk("A");
        task_yield();
    }
    task_exit();
}

static void test_task_b(void) {
    for (int i = 0; i < 5; i++) {
        printk("B");
        task_yield();
    }
    task_exit();
}
#endif

/* ==========================================================================
 * Initialization
 * ======================================================================= */
static int scheduler_init(void) {
    ready_list = NULL;
    current = NULL;

    task_init();

#if 0
    task_create(test_task_a, "test-a");
    task_create(test_task_b, "test-b");
    /* Start with first non-idle task */
    if (ready_list && ready_list->next != ready_list) {
        task_set_current(ready_list->next);
        ready_list->next->state = TASK_RUNNING;
    }
#endif

    printk("[scheduler] Round-robin scheduler initialized\n");
	return 0;
}

/* ==========================================================================
 * KScope node registration
 * ======================================================================= */
kscope_node_t scheduler_node = {
    .name = "x86-scheduler",
    .id = 0x0011,
    .class = KSCOPE_CLASS_TIME,
    .subclass = KSCOPE_SUBCLASS_TIME_SCHED,
    .requires = (kscope_node_t *[]){ &pit_timer_node, &x86_gdt_node, &heap_node },
    .require_count = 3,
    .provides = (const char *[]){"sched."},
    .provide_count = 1,
    .init = scheduler_init,
};