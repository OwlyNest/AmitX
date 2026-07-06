/*
	* include/arch/x86/task.h - Task abstraction
	* Author:   amity
	* Date:     Sat Jul  4 01:33:09 2026
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
#ifndef __TASK__
#define __TASK__

#define TASK_NAME_LEN        32
#define TASK_STACK_SIZE      4096
#define TASK_MAX_TASKS       64
#define TASK_DEFAULT_QUANTUM 10
#define TASK_NO_TIMEOUT      0

/* --- Includes ---*/
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

typedef uint32_t pid_t;
typedef uint32_t tid_t;

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_TERMINATED
} task_state_t;

typedef enum {
    SCHED_KERNEL,
    SCHED_LATENCY,
    SCHED_THROUGHPUT,
    SCHED_NORMAL
} sched_class_t;

typedef enum {
    WAKE_NONE,
    WAKE_SIGNALED,
    WAKE_TIMEOUT
} wake_reason_t;

typedef struct task {
    /* Identity */
    tid_t tid;
    pid_t pid;
    char name[TASK_NAME_LEN];

    /* Scheduling */
    task_state_t state;
    sched_class_t sched_class;
    uint32_t quantum;
    uint32_t sleep_until;

    /* Saved CPU context (callee-saved + eflags + esp) */
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t eflags;

    /* Address space (future) */
    uint32_t cr3;

    /* Kernel stack */
    void *kernel_stack;
    uint32_t kernel_stack_size;

    /* Scheduler links */
    struct task *next;
    struct task *prev;

	void (*entry)(void *argument);
    void *arg;

    /* Wait queue linkage (mutex / semaphore blocking) */
    struct task *wait_next;
    struct task *wait_prev;
    struct task_queue *wait_queue;
    wake_reason_t wake_reason;
} task_t;

typedef struct task_queue {
    task_t *head;
    task_t *tail;
} task_queue_t;

typedef struct scheduler_class {
    void (*enqueue)(task_t *);
    task_t *(*pick_next)(void);
    void (*tick)(task_t *);
} scheduler_class_t;

/* --- Globals ---*/

/* --- Prototypes ---*/

/* ==========================================================================
 * Lifecycle
 * ======================================================================= */
void task_init(void);
task_t *task_create(void (*entry)(void), const char *name);
void task_destroy(task_t *task);
void task_exit(void);

/* ==========================================================================
 * Current task
 * ======================================================================= */
task_t *task_current(void);
void task_set_current(task_t *task);

/* ==========================================================================
 * Sleep and wake
 * ======================================================================= */
void task_sleep(uint32_t ms);
void task_wake(tid_t tid);
void task_wake_all(void);

/* ==========================================================================
 * Yield and schedule
 * ======================================================================= */
void task_yield(void);
void schedule(void);

/* ==========================================================================
 * Lookup
 * ======================================================================= */
task_t *task_find(tid_t tid);
int task_count(void);

/* ==========================================================================
 * Argument-based creation (for callbacks needing context, e.g. ACPICA)
 * ======================================================================= */
task_t *task_create_arg(void (*entry)(void *), void *arg, const char *name);

/* ==========================================================================
 * Wait queues — building block for mutexes and semaphores
 * ======================================================================= */
void task_queue_init(task_queue_t *q);
void task_queue_push(task_queue_t *q, task_t *task);
task_t *task_queue_pop(task_queue_t *q);
void task_queue_remove(task_queue_t *q, task_t *task);
wake_reason_t task_block_on(task_queue_t *q, uint32_t timeout_ms);
void task_wake_one(task_queue_t *q);

#endif