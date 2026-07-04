/*
	* include/arch/x86/task.h - [Enter description]
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

#define TASK_NAME_LEN   32
#define TASK_STACK_SIZE 4096
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

typedef struct task {
	/* Identity */
    tid_t tid;
    pid_t pid;

	int ring;
	sched_class_t sched_class;

    char name[TASK_NAME_LEN];

    /* Scheduling */
    task_state_t state;
    uint32_t quantum;
    uint32_t sleep_until;

    /* Saved CPU context */
    uint32_t esp;

    /* Kernel stack */
    void *kernel_stack;
    uint32_t kernel_stack_size;

    /* Address space (future) */
    uint32_t cr3;

    /* Scheduler links */
    struct task *next;
    struct task *prev;
} task_t;

typedef struct scheduler_class {
    void (*enqueue)(task_t *);
    task_t *(*pick_next)(void);
    void (*tick)(task_t *);
} scheduler_class_t;
/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Main ---*/
void task_init(void);
task_t *task_create(void (*entry)(void), const char *name);
void task_destroy(task_t *task);

task_t *task_current(void);
void task_set_current(task_t *task);
#endif