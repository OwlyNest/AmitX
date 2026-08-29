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
static void reaper_entry(void);
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
 * Reaper task — cleans up zombie tasks
 * ======================================================================= */
static void reaper_entry(void) {
    while (1) {
        scheduler_reap();
        task_sleep(50); /* Reap every 500ms */
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
 * Map sched_class to base priority and quantum
 * ======================================================================= */
// static uint8_t class_to_prio(sched_class_t cls) {
//     switch (cls) {
//         case SCHED_KERNEL: return TASK_PRIO_CRITICAL;
//         case SCHED_LATENCY: return TASK_PRIO_HIGHEST;
//         case SCHED_THROUGHPUT: return TASK_PRIO_NORMAL;
//         case SCHED_NORMAL: return TASK_PRIO_NORMAL;
//         default: return TASK_PRIO_NORMAL;
//     }
// }

static uint32_t prio_to_quantum(uint8_t prio) {
    /* Interactive (high prio) gets short quantum for responsiveness.
     * Background (low prio) gets long quantum for throughput.
    */
    if (prio >= TASK_PRIO_REALTIME) {
        return 6; /* RT tasks, generous but preemtable */
    } else if (prio >= TASK_PRIO_ABOVE_NORM) {
        return 2; /* Interractive: snappy */
    } else if (prio >= TASK_PRIO_NORMAL) {
        return 4; /* Normal: balanced */
    } else {
        return 8; /* Background: batch-like */
    }
}

/* ==========================================================================
 * Initialization
 * ======================================================================= */
void task_init(void) {
    memset(task_table, 0, sizeof(task_table));
    current_task = NULL;
    next_tid = 1;

    /* Create idle task -- always exists, always ready */
    idle_task = task_create(idle_entry, "idle");
    if (idle_task) {
        idle_task->sched_class = SCHED_KERNEL;
        idle_task->base_prio = TASK_PRIO_IDLE;
        idle_task->cur_prio = TASK_PRIO_IDLE;
        idle_task->quantum = 0xFFFFFFFF; /* Idle never preempts */
        idle_task->quantum_max = 0xFFFFFFFF;
    }
    /* Create reaper task -- high priority, cleans up exited tasks */
    task_t *reaper = task_create_prio(reaper_entry, "reaper", TASK_PRIO_CRITICAL);

    if (reaper) {
        reaper->sched_class = SCHED_KERNEL;
    }
}

/* ==========================================================================
 * Create a new task (legacy, normal priority)
 * ======================================================================= */
task_t *task_create(void (*entry)(void), const char *name) {
    return task_create_arg_prio(task_legacy_shim, (void *)entry,
                                name, TASK_PRIO_NORMAL);
}

/* ==========================================================================
 * Create a new task with specific priority (legacy entry)
 * ======================================================================= */
task_t *task_create_prio(void (*entry)(void), const char *name,
                         uint8_t priority) {
    return task_create_arg_prio(task_legacy_shim, (void *)entry,
                                name, priority);
}

/* ==========================================================================
 * Create a new task with an argument passed to entry
 * ======================================================================= */
task_t *task_create_arg(void (*entry)(void *), void *arg,
                        const char *name) {
    return task_create_arg_prio(entry, arg, name, TASK_PRIO_NORMAL);
}

/* ==========================================================================
 * Create a new task with argument and priority
 * ======================================================================= */
task_t *task_create_arg_prio(void (*entry)(void *), void *arg,
                             const char *name, uint8_t priority) {
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

    /* Clamp priority to valid range */
    if (priority > TASK_PRIO_CRITICAL) priority = TASK_PRIO_CRITICAL;
    task->base_prio = priority;
    task->cur_prio = priority;

    task->quantum_max = prio_to_quantum(priority);
    task->quantum = task->quantum_max;
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
    *--stack = 0;                           /* ebp */
    *--stack = 0;                           /* ebx */
    *--stack = 0;                           /* esi */
    *--stack = 0;                           /* edi */
    *--stack = 0x202;                       /* eflags */

    task->esp = (uint32_t)stack;

    task_table[slot] = task;

    scheduler_add(task);

    printk("[task] Created '%s' tid=%u prio=%u slot=%d\n", task->name, task->tid, task->cur_prio, slot);

    return task;
}

/* ==========================================================================
 * Destroy a task
 * ======================================================================= */
void task_destroy(task_t *task) {
    if (!task) return;

    /* Remove from scheduler and timer wheel */
    scheduler_remove(task);
    timer_wheel_remove(task);

    /* Remove from any wait queue */
    if (task->wait_queue) {
        task_queue_remove(task->wait_queue, task);
    }

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
        task->quantum = task->quantum_max;
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
    /* PIT is at 100 Hz -> 10 ms per tick. Convert ms to ticks,
     * rounding up so sleep(1) sleeps at least one tick.
    */
    uint32_t ticks = (ms + 9) / 10;
    if (ticks == 0) ticks = 1;

    task->sleep_until = tick_count + ticks;
    task->state = TASK_SLEEPING;

    /* Insert into timer wheel for efficient wake */
    timer_wheel_insert(task, ticks);

    schedule();
}

/* ==========================================================================
 * Wake a sleeping task by tid
 * ======================================================================= */
void task_wake(tid_t tid) {
    task_t *task = task_find(tid);
    if (task && task->state == TASK_SLEEPING) {
        timer_wheel_remove(task);
        task->state = TASK_READY;
        task->sleep_until = 0;
        scheduler_add(task);
    }

}

/* ==========================================================================
 * Wake all sleeping tasks
 * ======================================================================= */
void task_wake_all(void) {
    for (int i = 0; i < TASK_MAX_TASKS; i++) {
        task_t *t = task_table[i];
        if (t && t->state == TASK_SLEEPING) {
            timer_wheel_remove(t);
            t->state = TASK_READY;
            t->sleep_until = 0;
            scheduler_add(t);
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

    task->state = TASK_ZOMBIE;
    printk("[task] %s exited (zombie)\n", task->name);

    /* Schedule away -- reaper will clean us up later */
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
 * Priority management
 * ======================================================================= */
void task_boost_priority(task_t *task, uint8_t new_prio) {
    if (!task) return;
    if (new_prio > TASK_PRIO_CRITICAL) new_prio = TASK_PRIO_CRITICAL;

    /* Only boost dynamic priorities (1-15). RT priorities (16+)
     * are fixed and should not be altered.
    */
    if (task->base_prio >= TASK_PRIO_REALTIME) return;
    if (new_prio <= task->base_prio) return; /* Don't lower priority */

    uint8_t old_prio = task->cur_prio;
    task->cur_prio = new_prio;

    /* If task is on a ready queue, move it to the new prio list */
    if (task->state == TASK_READY) { // Running tasks are now queue'd
        scheduler_remove(task);
        scheduler_add(task);
    }

    printk("[task] Boost %s: %u -> %u\n", task->name, old_prio, task->cur_prio);
}

void task_unboost_priority(task_t *task) {
    if (!task) return;
    if (task->cur_prio == task->base_prio) return; /* Can't move under base priority */

    uint8_t old_prio = task->cur_prio;
    task->cur_prio = task->base_prio;

    if (task->state == TASK_READY) {
        scheduler_remove(task);
        scheduler_add(task);
    }

    printk("[task] Boost %s: %u -> %u\n", task->name, old_prio, task->cur_prio);
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
	task->wait_queue = q;
}

void task_queue_remove(task_queue_t *q, task_t *task) {
	if (!q || !task) return;
    if (task->wait_queue != q) return;

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
 * Wake all tasks from a wait queue
 * ======================================================================= */
void task_wake_all_queue(task_queue_t *q) {
    if (!q) return;
    while (q->head) {
        task_t *t = task_queue_pop(q);
        if (!t) break;
        t->wake_reason = WAKE_SIGNALED;
        t->sleep_until = 0;
        t->state = TASK_READY;
        scheduler_add(t);
    }
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

    uint32_t timeout_ticks = 0;
    if (timeout_ms != TASK_NO_TIMEOUT) {
        timeout_ticks = (timeout_ms + 9) / 10;
        if (timeout_ticks == 0) timeout_ticks = 1;
        task->sleep_until = tick_count + timeout_ticks;
    } else {
        task->sleep_until = 0;
    }

	task_queue_push(q, task);

    if (timeout_ticks > 0) {
        timer_wheel_insert(task, timeout_ticks);
    }

	schedule();

	return task->wake_reason;
}
/* ==========================================================================
 * Wake a single task from a wait queue (FIFO)
 * ======================================================================= */
void task_wake_one(task_queue_t *q) {
	task_t *task = task_queue_pop(q);
    if (!task) return;

    timer_wheel_remove(task);
    task->wake_reason = WAKE_SIGNALED;
    task->sleep_until = 0;
    task->state = TASK_READY;
    scheduler_add(task);
}

task_t **task_get_table(void) {
    return task_table;
}