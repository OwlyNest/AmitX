/*
	* arch/x86/task.c - Task lifecycle and management
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
#include <arch/x86/scheduler.h>
#include <mm/heap.h>
#include <lib/string.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static task_t *current_task = NULL;
static tid_t next_tid = 1;
static task_t *task_table[TASK_MAX_TASKS] = { 0 };
static task_t *idle_task = NULL;

/* --- Prototypes ---*/
static void idle_entry(void);
static int task_find_free_slot(void);
static void task_trampoline(void);
static void task_legacy_shim(void *arg);

/* --- Functions ---*/

/* ==========================================================================
 * Idle task — runs when nothing else is ready
 * ======================================================================= */
static void idle_entry(void) {
    while (1) {
        asm volatile ("hlt");
    }
}

/* ==========================================================================
 * Internal helpers
 * ======================================================================= */
static int task_find_free_slot(void) {
    for (int i = 0; i < TASK_MAX_TASKS; i++) {
        if (!task_table[i]) return i;
    }
    return -1;
}

/* ==========================================================================
 * Trampoline — real ret-target for every task; unpacks entry+arg
 * ======================================================================= */
static void task_trampoline(void) {
	task_t *self = task_current();
	self->entry(self->arg);
	task_exit();
}

/* ==========================================================================
 * Shim so legacy void(*)(void) entries still work unchanged
 * ======================================================================= */
static void task_legacy_shim(void *arg) {
	void (*fn)(void) = (void (*)(void))arg;
    fn();
}

/* ==========================================================================
 * Initialization
 * ======================================================================= */
void task_init(void) {
    memset(task_table, 0, sizeof(task_table));
    current_task = NULL;
    next_tid = 1;

    /* Create idle task — always exists, always ready */
    idle_task = task_create(idle_entry, "idle");
    if (idle_task) {
        idle_task->sched_class = SCHED_KERNEL;
        idle_task->quantum = 0xFFFFFFFF; /* Idle never preempts */
    }
}

/* ==========================================================================
 * Create a new task
 * ======================================================================= */
task_t *task_create(void (*entry)(void), const char *name) {
    return task_create_arg(task_legacy_shim, (void *)entry, name);
}

/* ==========================================================================
 * Create a new task with an argument passed to entry
 * ======================================================================= */
task_t *task_create_arg(void (*entry)(void *), void *arg, const char *name) {
	if (!entry) return NULL;

	int slot = task_find_free_slot();
	if (slot < 0) {
		printk("[task] No free task slots\n");
        return NULL;
	}

	task_t *task = (task_t *)malloc(sizeof(task_t));
    if (!task) return NULL;

	memset(task, 0, sizeof(task_t));

	task->tid = next_tid++;
    task->pid = task->tid;
    task->state = TASK_READY;
    task->sched_class = SCHED_NORMAL;
    task->quantum = TASK_DEFAULT_QUANTUM;
    task->sleep_until = 0;
    task->entry = entry;
    task->arg = arg;
    task->wake_reason = WAKE_NONE;

	if (name) {
		strncpy(task->name, name, TASK_NAME_LEN - 1);
		task->name[TASK_NAME_LEN - 1] = '\0';
	}

	task->eflags = 0x202;
    task->ebp = 0;
    task->ebx = 0;
    task->esi = 0;
    task->edi = 0;
    task->cr3 = 0;

    task->kernel_stack_size = TASK_STACK_SIZE;
    task->kernel_stack = malloc(TASK_STACK_SIZE);
    if (!task->kernel_stack) {
        free(task);
        return NULL;
    }

	uint32_t *stack = (uint32_t *)((uint8_t *)task->kernel_stack + TASK_STACK_SIZE);

	*--stack = (uint32_t)task_exit;         /* fallback if trampoline rets */
    *--stack = (uint32_t)task_trampoline;   /* context_switch ret target */
    *--stack = 0;                            /* ebp */
    *--stack = 0;                            /* ebx */
    *--stack = 0;                            /* esi */
    *--stack = 0;                            /* edi */
    *--stack = 0x202;                        /* eflags */

	task->esp = (uint32_t)stack;

	task_table[slot] = task;

	scheduler_add(task);

	printk("[task] Created '%s' tid = %u slot = %d\n", task->name, task->tid, slot);

	return task;
}

/* ==========================================================================
 * Destroy a task
 * ======================================================================= */
void task_destroy(task_t *task) {
    if (!task) return;

    scheduler_remove(task);

    for (int i = 0; i < TASK_MAX_TASKS; i++) {
        if (task_table[i] == task) {
            task_table[i] = NULL;
            break;
        }
    }

    if (task->kernel_stack) {
        free(task->kernel_stack);
    }

    printk("[task] Destroyed %s (tid=%u)\n", task->name, task->tid);

    free(task);
}

/* ==========================================================================
 * Current task accessors
 * ======================================================================= */
task_t *task_current(void) {
    return current_task;
}

void task_set_current(task_t *task) {
    current_task = task;
}

/* ==========================================================================
 * Yield CPU to next ready task
 * ======================================================================= */
void task_yield(void) {
    task_t *task = task_current();
    if (task) {
        task->quantum = TASK_DEFAULT_QUANTUM;
    }
    schedule();
}

/* ==========================================================================
 * Sleep for milliseconds
 * ======================================================================= */
void task_sleep(uint32_t ms) {
    task_t *task = task_current();
    if (!task) return;

    extern volatile uint32_t tick_count;
    task->sleep_until = tick_count + ms;
    task->state = TASK_SLEEPING;
    schedule();
}

/* ==========================================================================
 * Wake a sleeping task by tid
 * ======================================================================= */
void task_wake(tid_t tid) {
    task_t *task = task_find(tid);
    if (task && task->state == TASK_SLEEPING) {
        task->state = TASK_READY;
        task->sleep_until = 0;
    }
}

/* ==========================================================================
 * Wake all sleeping tasks
 * ======================================================================= */
void task_wake_all(void) {
    for (int i = 0; i < TASK_MAX_TASKS; i++) {
        task_t *t = task_table[i];
        if (t && t->state == TASK_SLEEPING) {
            t->state = TASK_READY;
            t->sleep_until = 0;
        }
    }
}

/* ==========================================================================
 * Task exit — called when task entry returns
 * ======================================================================= */
void task_exit(void) {
    task_t *task = task_current();
    if (!task) {
        for (;;) asm volatile ("hlt");
    }

    task->state = TASK_TERMINATED;
    printk("[task] %s exited\n", task->name);

    /* Schedule away — this task will be reaped later */
    schedule();

    /* Should never reach here */
    for (;;) asm volatile ("hlt");
}

/* ==========================================================================
 * Lookup
 * ======================================================================= */
task_t *task_find(tid_t tid) {
    for (int i = 0; i < TASK_MAX_TASKS; i++) {
        task_t *t = task_table[i];
        if (t && t->tid == tid) return t;
    }
    return NULL;
}

int task_count(void) {
    int count = 0;
    for (int i = 0; i < TASK_MAX_TASKS; i++) {
        if (task_table[i]) count++;
    }
    return count;
}


/* ==========================================================================
 * Wait queue management
 * ======================================================================= */
void task_queue_init(task_queue_t *q) {
	if (!q) return;
	q->head = NULL;
	q->tail = NULL;
}

void task_queue_push(task_queue_t *q, task_t *task) {
	if (!q || !task) return;

	task->wait_next = NULL;
	task->wait_prev = q->tail;

	if (q->tail) {
		q->tail->wait_next = task;
	} else {
		q->head = task;
	}

	q->tail = task;
	task->wait_queue = 0;
}

void task_queue_remove(task_queue_t *q, task_t *task) {
	if (!q || !task) return;

	if (task->wait_prev) {
		task->wait_prev->wait_next = task->wait_next;
	} else {
		q->head = task->wait_next;
	}

	if (task->wait_next) {
		task->wait_next->wait_prev = task->wait_prev;
	} else {
		q->tail = task->wait_prev;
	}

	task->wait_next = NULL;
	task->wait_prev = NULL;
	task->wait_queue = NULL;
}

task_t *task_queue_pop(task_queue_t *q) {
	if (!q || !q->head) return NULL;

	task_t *task = q->head;
	task_queue_remove(q, task);

	return task;
}

/* ==========================================================================
 * Block current task on a wait queue, with optional timeout (ms)
 * ======================================================================= */
wake_reason_t task_block_on(task_queue_t *q, uint32_t timeout_ms) {
	extern volatile uint32_t tick_count;

	task_t *task = task_current();
	if (!task) return WAKE_NONE;

	task->state = TASK_BLOCKED;
	task->wake_reason = WAKE_NONE;
	task->sleep_until = (timeout_ms == TASK_NO_TIMEOUT) ? 0 : tick_count + timeout_ms;

	task_queue_push(q, task);

	schedule();

	return task->wake_reason;
}
/* ==========================================================================
 * Wake a single task from a wait queue (FIFO)
 * ======================================================================= */
void task_wake_one(task_queue_t *q) {
	task_t *task = task_queue_pop(q);
    if (!task) return;

    task->wake_reason = WAKE_SIGNALED;
    task->sleep_until = 0;
    task->state = TASK_READY;
}