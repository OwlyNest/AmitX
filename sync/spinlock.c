/*
 * sync/spinlock.c - Spinlock implementation
 * Author:   amity
 * Date:     Sun Jul  5 14:44:41 2026
 * Copyright © 2026 OwlyNest
 */

/* --- Styling Instructions ---
 * Encoding:                      UTF-8, Unix line endings
 * Text font:                     Monospace
 * Line width:                    Max 80 characters
 * Indentation:                   4 spaces
 * Brace style:                   Same line as control statement
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers: Use 3 '-' characters before and after
 * Pointer notation:              Next to variable name, not type
 * Binary operations:             Space around operator
 * Empty parameter list:          Use (void) instead of ()
 * Statements and declarations:   Max one per line
 */

/* --- Macros ---*/

/* --- Includes ---*/

#include <sync/spinlock.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

VOID spinlock_init(_SPINLOCK *lock) { lock->value = 0; }

#if ARCH_X86_64

static inline _SPINLOCK_FLAGS spinlock_irq_save(VOID) {
  _SPINLOCK_FLAGS flags;

  asm volatile("pushfq\n\t"
               "popq %0\n\t"
               "cli"
               : "=r"(flags)
               :
               : "memory", "cc");

  return flags;
}

static inline VOID spinlock_irq_restore(_SPINLOCK_FLAGS flags) {
  asm volatile("pushq %0\n\t"
               "popfq"
               :
               : "r"(flags)
               : "memory", "cc");
}

#else

static inline _SPINLOCK_FLAGS spinlock_irq_save(VOID) {
  _SPINLOCK_FLAGS flags;

  asm volatile("pushfl\n\t"
               "popl %0\n\t"
               "cli"
               : "=r"(flags)
               :
               : "memory", "cc");

  return flags;
}

static inline VOID spinlock_irq_restore(_SPINLOCK_FLAGS flags) {
  asm volatile("pushl %0\n\t"
               "popfl"
               :
               : "r"(flags)
               : "memory", "cc");
}

#endif

_SPINLOCK_FLAGS spinlock_acquire(_SPINLOCK *lock) {
  _SPINLOCK_FLAGS flags;

  flags = spinlock_irq_save();

  for (;;) {
    LONG old;

    old = 0;

    asm volatile("lock cmpxchgl %2, %1"
                 : "+a"(old), "+m"(lock->value)
                 : "r"((LONG)1)
                 : "memory", "cc");

    if (old == 0)
      break;

    while (lock->value != 0)
      asm volatile("pause");
  }

  return flags;
}

VOID spinlock_release(_SPINLOCK *lock, _SPINLOCK_FLAGS flags) {
  asm volatile("movl $0, %0" : "=m"(lock->value) : : "memory");

  spinlock_irq_restore(flags);
}