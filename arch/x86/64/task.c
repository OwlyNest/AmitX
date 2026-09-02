/*
 * arch/x86/64/task.c - [Enter description]
 * Author:   amity
 * Date:     Mon Aug 31 15:23:12 2026
 * Copyright © 2026 OwlyNest
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

/*
 * arch/x86/64/task.c - Task lifecycle and management (64-bit)
 * Author:   amity
 * Date:     Mon Aug 31 15:25:00 2026
 * Copyright © 2026 OwlyNest
 */

/* --- Includes ---*/
#include <arch/x86/scheduler.h>
#include <arch/x86/task.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Globals ---*/
static task_t *current_task = NULL;
static tid_t next_tid = 1;
static task_t *task_table[TASK_MAX_TASKS] = {0};
static task_t *idle_task = NULL;

/* --- Prototypes ---*/
static void idle_entry(void);
static void reaper_entry(void);
static int task_find_free_slot(void);
static void task_trampoline(void);
static void task_legacy_shim(void *arg);

/* --- Functions ---*/

static void idle_entry(void) {
  while (1) {
    asm volatile("hlt");
  }
}

static void reaper_entry(void) {
  while (1) {
    scheduler_reap();
    task_sleep(50);
  }
}

static int task_find_free_slot(void) {
  for (int i = 0; i < TASK_MAX_TASKS; i++) {
    if (!task_table[i])
      return i;
  }
  return -1;
}

static void task_trampoline(void) {
  task_t *self = task_current();
  self->entry(self->arg);
  task_exit();
}

static void task_legacy_shim(void *arg) {
  void (*fn)(void) = (void (*)(void))arg;
  fn();
}

static uint32_t prio_to_quantum(uint8_t prio) {
  if (prio >= TASK_PRIO_REALTIME) {
    return 6;
  } else if (prio >= TASK_PRIO_ABOVE_NORM) {
    return 2;
  } else if (prio >= TASK_PRIO_NORMAL) {
    return 4;
  } else {
    return 8;
  }
}

void task_init(void) {
  memset(task_table, 0, sizeof(task_table));
  current_task = NULL;
  next_tid = 1;

  idle_task = task_create(idle_entry, "idle");
  if (idle_task) {
    idle_task->sched_class = SCHED_KERNEL;
    idle_task->base_prio = TASK_PRIO_IDLE;
    idle_task->cur_prio = TASK_PRIO_IDLE;
    idle_task->quantum = 0xFFFFFFFF;
    idle_task->quantum_max = 0xFFFFFFFF;
  }

  task_t *reaper = task_create_prio(reaper_entry, "reaper", TASK_PRIO_CRITICAL);
  if (reaper) {
    reaper->sched_class = SCHED_KERNEL;
  }
}

task_t *task_create(void (*entry)(void), const char *name) {
  return task_create_arg_prio(task_legacy_shim, (void *)entry, name,
                              TASK_PRIO_NORMAL);
}

task_t *task_create_prio(void (*entry)(void), const char *name,
                         uint8_t priority) {
  return task_create_arg_prio(task_legacy_shim, (void *)entry, name, priority);
}

task_t *task_create_arg(void (*entry)(void *), void *arg, const char *name) {
  return task_create_arg_prio(entry, arg, name, TASK_PRIO_NORMAL);
}

task_t *task_create_arg_prio(void (*entry)(void *), void *arg, const char *name,
                             uint8_t priority) {
  if (!entry)
    return NULL;

  int slot = task_find_free_slot();
  if (slot < 0) {
    printk("[task] No free task slots\n");
    return NULL;
  }

  task_t *task = (task_t *)malloc(sizeof(task_t));
  if (!task)
    return NULL;

  memset(task, 0, sizeof(task_t));

  task->tid = next_tid++;
  task->pid = task->tid;
  task->state = TASK_READY;
  task->sched_class = SCHED_NORMAL;

  if (priority > TASK_PRIO_CRITICAL)
    priority = TASK_PRIO_CRITICAL;
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

  /* Set up initial CPU context for 64-bit */
  task->context.rflags = 0x202;
  task->context.rbp = 0;
  task->context.rbx = 0;
  task->context.r12 = 0;
  task->context.r13 = 0;
  task->context.r14 = 0;
  task->context.r15 = 0;
  task->context.rdi = 0;
  task->context.rsi = 0;
  task->context.rip = (QWORD)task_trampoline;
  task->cr3 = 0;

  task->kernel_stack_size = TASK_STACK_SIZE;
  task->kernel_stack = malloc(TASK_STACK_SIZE);
  if (!task->kernel_stack) {
    free(task);
    return NULL;
  }

  /* Build initial stack for 64-bit context_switch:
   *
   * context_switch expects to pop (in reverse order of push):
   *   rflags, rsi, rdi, r15, r14, r13, r12, rbx, rbp
   * then ret.
   *
   * We push:
   *   task_trampoline  (ret target)
   *   task_exit        (fallback if trampoline returns)
   *   rbp              (0)
   *   rbx              (0)
   *   r12              (0)
   *   r13              (0)
   *   r14              (0)
   *   r15              (0)
   *   rdi              (0)
   *   rsi              (0)
   *   rflags           (0x202 = IF set)
   */
  QWORD *stack = (QWORD *)((uint8_t *)task->kernel_stack + TASK_STACK_SIZE);

  *--stack = (QWORD)task_exit;       /* Fallback */
  *--stack = (QWORD)task_trampoline; /* ret target */
  *--stack = 0;                      /* rbp */
  *--stack = 0;                      /* rbx */
  *--stack = 0;                      /* r12 */
  *--stack = 0;                      /* r13 */
  *--stack = 0;                      /* r14 */
  *--stack = 0;                      /* r15 */
  *--stack = 0;                      /* rdi */
  *--stack = 0;                      /* rsi */
  *--stack = 0x202;                  /* rflags */

  task->context.rsp = (QWORD)stack;

  task_table[slot] = task;
  scheduler_add(task);

  printk("[task] Created '%s' tid=%u prio=%u slot=%d\n", task->name, task->tid,
         task->cur_prio, slot);

  return task;
}

/* --- Everything below here is identical to 32-bit --- */
/* (In a real refactor, extract to common/task_common.c) */

void task_destroy(task_t *task) {
  if (!task)
    return;

  scheduler_remove(task);
  timer_wheel_remove(task);

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

task_t *task_current(void) { return current_task; }

void task_set_current(task_t *task) { current_task = task; }

void task_yield(void) {
  task_t *task = task_current();
  if (task) {
    task->quantum = task->quantum_max;
  }
  schedule();
}

void task_sleep(uint32_t ms) {
  task_t *task = task_current();
  if (!task)
    return;

  extern volatile uint32_t tick_count;
  uint32_t ticks = (ms + 9) / 10;
  if (ticks == 0)
    ticks = 1;

  task->sleep_until = tick_count + ticks;
  task->state = TASK_SLEEPING;

  timer_wheel_insert(task, ticks);
  schedule();
}

void task_wake(tid_t tid) {
  task_t *task = task_find(tid);
  if (task && task->state == TASK_SLEEPING) {
    timer_wheel_remove(task);
    task->state = TASK_READY;
    task->sleep_until = 0;
    scheduler_add(task);
  }
}

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

void task_exit(void) {
  task_t *task = task_current();
  if (!task) {
    for (;;)
      asm volatile("hlt");
  }

  task->state = TASK_ZOMBIE;
  printk("[task] %s exited (zombie)\n", task->name);
  schedule();

  for (;;)
    asm volatile("hlt");
}

task_t *task_find(tid_t tid) {
  for (int i = 0; i < TASK_MAX_TASKS; i++) {
    task_t *t = task_table[i];
    if (t && t->tid == tid)
      return t;
  }
  return NULL;
}

int task_count(void) {
  int count = 0;
  for (int i = 0; i < TASK_MAX_TASKS; i++) {
    if (task_table[i])
      count++;
  }
  return count;
}

void task_boost_priority(task_t *task, uint8_t new_prio) {
  if (!task)
    return;
  if (new_prio > TASK_PRIO_CRITICAL)
    new_prio = TASK_PRIO_CRITICAL;
  if (task->base_prio >= TASK_PRIO_REALTIME)
    return;
  if (new_prio <= task->base_prio)
    return;

  uint8_t old_prio = task->cur_prio;
  task->cur_prio = new_prio;

  if (task->state == TASK_READY) {
    scheduler_remove(task);
    scheduler_add(task);
  }

  printk("[task] Boost %s: %u -> %u\n", task->name, old_prio, task->cur_prio);
}

void task_unboost_priority(task_t *task) {
  if (!task)
    return;
  if (task->cur_prio == task->base_prio)
    return;

  uint8_t old_prio = task->cur_prio;
  task->cur_prio = task->base_prio;

  if (task->state == TASK_READY) {
    scheduler_remove(task);
    scheduler_add(task);
  }

  printk("[task] Boost %s: %u -> %u\n", task->name, old_prio, task->cur_prio);
}

void task_queue_init(task_queue_t *q) {
  if (!q)
    return;
  q->head = NULL;
  q->tail = NULL;
}

void task_queue_push(task_queue_t *q, task_t *task) {
  if (!q || !task)
    return;

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
  if (!q || !task)
    return;
  if (task->wait_queue != q)
    return;

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
  if (!q || !q->head)
    return NULL;

  task_t *task = q->head;
  task_queue_remove(q, task);
  return task;
}

void task_wake_all_queue(task_queue_t *q) {
  if (!q)
    return;
  while (q->head) {
    task_t *t = task_queue_pop(q);
    if (!t)
      break;
    t->wake_reason = WAKE_SIGNALED;
    t->sleep_until = 0;
    t->state = TASK_READY;
    scheduler_add(t);
  }
}

wake_reason_t task_block_on(task_queue_t *q, uint32_t timeout_ms) {
  extern volatile uint32_t tick_count;

  task_t *task = task_current();
  if (!task)
    return WAKE_NONE;

  task->state = TASK_BLOCKED;
  task->wake_reason = WAKE_NONE;

  uint32_t timeout_ticks = 0;
  if (timeout_ms != TASK_NO_TIMEOUT) {
    timeout_ticks = (timeout_ms + 9) / 10;
    if (timeout_ticks == 0)
      timeout_ticks = 1;
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

void task_wake_one(task_queue_t *q) {
  task_t *task = task_queue_pop(q);
  if (!task)
    return;

  timer_wheel_remove(task);
  task->wake_reason = WAKE_SIGNALED;
  task->sleep_until = 0;
  task->state = TASK_READY;
  scheduler_add(task);
}

task_t **task_get_table(void) { return task_table; }