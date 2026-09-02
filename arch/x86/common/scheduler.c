/*
 * arch/x86/common/scheduler.c - Priority-based round-robin scheduler
 * Author:   amity
 * Date:     Sat Jul  4 13:00:37 2026
 * Copyright (c) 2026 OwlyNest
 */

/* --- Styling Instructions ---
 * Encoding:                      UTF-8, Unix line endings
 * Text font:                     Monospace
 * Line width:                    Max 80 characters
 * Indentation:                   Use 4 spaces
 * Brace style:                   Same line as control statement
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
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
/* One ready queue per priority level. Head of each list.
 * Tasks within a priority are FIFO (insert at tail, pick from head).
 */
static task_t *ready_queue[TASK_NUM_PRIOS] = {0};
static task_t *ready_tail[TASK_NUM_PRIOS] = {0};

/* Bitmap: bit N set if ready_queue[N] is non-empty.
 * Makes finding highest priority O(1) instead of O(32).
 */
static uint32_t ready_bitmap = 0;

/* Current running task */
static task_t *current = NULL;

/* Timer wheel: 256 buckets, each tick advances one bucket.
 * Tasks hash into a bucket based on (tick_count + timeout) % 256.
 * Collisions chain via timer_next/timer_prev.
 * This is a classic "hashed timer wheel" -- Linux uses a variant.
 */
#define TIMER_WHEEL_SIZE 256
static task_t *timer_wheel[TIMER_WHEEL_SIZE] = {0};

extern void context_switch(task_t *old, task_t *new);
extern volatile uint32_t tick_count;

/* --- Prototypes ---*/
static inline int fls(uint32_t word);

/* --- Functions ---*/

/* ==========================================================================
 * Find last set bit (index of highest set bit, 1-based).
 * On x86, this is the BSR instruction. We use it to find the
 * highest priority ready queue in O(1) from the bitmap.
 * ======================================================================= */
static inline int fls(uint32_t word) {
  if (word == 0)
    return 0;
  int bit;
  asm volatile("bsr %1, %0" : "=r"(bit) : "r"(word));
  return bit + 1;
}

/* ==========================================================================
 * Add task to ready list at its current priority
 * ======================================================================= */
void scheduler_add(task_t *task) {
  if (!task)
    return;

  uint8_t prio = task->cur_prio;
  if (prio >= TASK_NUM_PRIOS)
    prio = TASK_NUM_PRIOS - 1;

  task->state = TASK_READY;
  task->next = NULL;
  task->prev = ready_tail[prio];

  if (ready_tail[prio]) {
    ready_tail[prio]->next = task;
  } else {
    ready_queue[prio] = task;
    ready_bitmap |= (1U << prio);
  }

  ready_tail[prio] = task;
}

/* ==========================================================================
 * Remove task from ready list
 * ======================================================================= */
void scheduler_remove(task_t *task) {
  if (!task)
    return;
  if (task->state != TASK_READY)
    return;

  uint8_t prio = task->cur_prio;
  if (prio >= TASK_NUM_PRIOS)
    prio = TASK_NUM_PRIOS - 1;

  if (task->prev) {
    task->prev->next = task->next;
  } else {
    ready_queue[prio] = task->next;
  }

  if (task->next) {
    task->next->prev = task->prev;
  } else {
    ready_tail[prio] = task->prev;
  }

  task->next = NULL;
  task->prev = NULL;

  if (!ready_queue[prio]) {
    ready_bitmap &= ~(1U << prio);
  }
}

/* ==========================================================================
 * Pick next ready task: highest priority, FIFO within priority
 * ======================================================================= */
task_t *scheduler_next(void) {
  if (ready_bitmap == 0) {
    /* Nothing ready -- return idle (prio 0, always there) */
    return ready_queue[TASK_PRIO_IDLE];
  }

  /* fls gives us 1-based index of highest set bit.
   * Subtract 1 for 0-based priority.
   */
  int highest = fls(ready_bitmap) - 1;
  if (highest < 0)
    highest = 0;

  task_t *next = ready_queue[highest];
  if (!next) {
    /* Paranoia: bitmap out of sync */
    ready_bitmap &= ~(1U << highest);
    return scheduler_next();
  }

  return next;
}

/* ==========================================================================
 * Timer tick handler -- called from PIT ISR
 * ======================================================================= */
void scheduler_tick(void) {
  /* Process timer wheel bucket for this tick */
  scheduler_timer_wheel_tick();

  /* Decrement current task quantum */
  task_t *task = task_current();
  if (!task)
    return;

  if (task->quantum > 0) {
    task->quantum--;
  }

  if (task->quantum == 0) {
    task->quantum = task->quantum_max;
    schedule();
  }
}

/* ==========================================================================
 * Schedule -- switch to next ready task
 * ======================================================================= */
void schedule(void) {
  task_t *old = task_current();
  task_t *next = scheduler_next();

  if (!next)
    return;
  if (old == next)
    return;

  if (old && old->state == TASK_RUNNING) {
    old->state = TASK_READY;
    /* Re-queue at tail of its priority for round-robin */
    scheduler_add(old);
  }

  /* Remove next from ready queue (it's about to run) */
  scheduler_remove(next);

  next->state = TASK_RUNNING;
  task_set_current(next);

  context_switch(old, next);
}

/* ==========================================================================
 * Timer wheel: insert a task to fire after 'timeout_ticks'
 * ======================================================================= */
void timer_wheel_insert(task_t *task, uint32_t timeout_ticks) {
  if (!task)
    return;

  /* Remove if already in wheel */
  timer_wheel_remove(task);

  uint32_t bucket = (tick_count + timeout_ticks) % TIMER_WHEEL_SIZE;
  task->timer_bucket = bucket;

  task->timer_next = timer_wheel[bucket];
  task->timer_prev = NULL;
  if (timer_wheel[bucket]) {
    timer_wheel[bucket]->timer_prev = task;
  }
  timer_wheel[bucket] = task;
}

/* ==========================================================================
 * Timer wheel: remove a task from its bucket
 * ======================================================================= */
void timer_wheel_remove(task_t *task) {
  if (!task)
    return;
  if (task->timer_bucket >= TIMER_WHEEL_SIZE)
    return;

  uint32_t bucket = task->timer_bucket;

  if (task->timer_prev) {
    task->timer_prev->timer_next = task->timer_next;
  } else {
    timer_wheel[bucket] = task->timer_next;
  }

  if (task->timer_next) {
    task->timer_next->timer_prev = task->timer_prev;
  }

  task->timer_next = NULL;
  task->timer_prev = NULL;
  task->timer_bucket = 0xFFFFFFFF;
}

/* ==========================================================================
 * Timer wheel: process the bucket for the current tick
 * Checks all tasks in this bucket. If their sleep_until has passed,
 * wake them. If not, re-insert them (they were hashed to this
 * bucket due to wrap-around -- this is the "cascading" behavior).
 * ======================================================================= */
void scheduler_timer_wheel_tick(void) {
  uint32_t bucket = tick_count % TIMER_WHEEL_SIZE;
  task_t *task = timer_wheel[bucket];

  while (task) {
    task_t *next = task->timer_next;

    if (task->sleep_until <= tick_count) {
      /* Timer expired -- wake the task */
      timer_wheel_remove(task);

      if (task->state == TASK_SLEEPING) {
        task->state = TASK_READY;
        task->sleep_until = 0;
        scheduler_add(task);
      } else if (task->state == TASK_BLOCKED) {
        if (task->wait_queue) {
          task_queue_remove(task->wait_queue, task);
        }
        task->wake_reason = WAKE_TIMEOUT;
        task->sleep_until = 0;
        task->state = TASK_READY;
        scheduler_add(task);
      }
      /* else: state changed, ignore */
    }
    /* If not expired, leave in bucket -- it'll be checked
     * again next time this bucket comes around (256 ticks later).
     * For short timeouts this is fine. For long timeouts,
     * a hierarchical wheel (Linux style) is better, but this
     * is sufficient for 256 tasks * 2.5s max resolution.
     */

    task = next;
  }
}

/* ==========================================================================
 * Reaper: clean up zombie tasks
 * ======================================================================= */
void scheduler_reap(void) {
  for (int i = 0; i < TASK_MAX_TASKS; i++) {
    task_t **task_table = task_get_table();
    task_t *t = task_table[i];
    if (t && t->state == TASK_ZOMBIE) {
      /* Don't reap the current task or idle */
      if (t == task_current())
        continue;
      if (t == ready_queue[TASK_PRIO_IDLE])
        continue;

      printk("[sched] Reaping %s (tid=%u)\n", t->name, t->tid);
      task_destroy(t);
    }
  }
}

/* ==========================================================================
 * Test tasks (enable for scheduler verification)
 * ======================================================================= */
#if 0
static void test_task_high(void) {
    for (int i = 0; i < 5; i++) {
        printk("H");
        task_yield();
    }
    task_exit();
}

static void test_task_low(void) {
    for (int i = 0; i < 5; i++) {
        printk("L");
        task_yield();
    }
    task_exit();
}
#endif

/* ==========================================================================
 * Initialization
 * ======================================================================= */
static int scheduler_init(void) {
  for (int i = 0; i < TASK_NUM_PRIOS; i++) {
    ready_queue[i] = NULL;
    ready_tail[i] = NULL;
  }
  ready_bitmap = 0;
  current = NULL;

  for (int i = 0; i < TIMER_WHEEL_SIZE; i++) {
    timer_wheel[i] = NULL;
  }

  task_init();

#if 0
    task_create_prio(test_task_high, "test-high", TASK_PRIO_HIGHEST);
    task_create_prio(test_task_low, "test-low", TASK_PRIO_LOWEST);
#endif

  printk("[scheduler] Priority scheduler initialized\n");
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
    .requires = (kscope_node_t *[]){&pit_timer_node, &x86_gdt_node, &heap_node},
    .require_count = 3,
    .provides = (const char *[]){"sched."},
    .provide_count = 1,
    .init = scheduler_init,
};