/*
 * acpi/acpi_os_threads.c - [Enter description]
 * Author:   amity
 * Date:     Sun Jul  5 15:08:07 2026
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

/* --- Includes ---*/
#include "acpi.h"
#include <arch/x86/task.h>
#include <mm/heap.h>
#include <sync/mutex.h>
#include <sync/semaphore.h>
#include <sync/spinlock.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

ACPI_THREAD_ID AcpiOsGetThreadId(void) {
  task_t *task = task_current();
  return task ? (ACPI_THREAD_ID)task->tid : (ACPI_THREAD_ID)1;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type,
                          ACPI_OSD_EXEC_CALLBACK Function, void *Context) {
  (void)Type;
  task_t *task =
      task_create_arg((void (*)(void *))Function, Context, "acpi-exec");
  return task ? AE_OK : AE_NO_MEMORY;
}

void AcpiOsSleep(UINT64 Milliseconds) { task_sleep((uint32_t)Milliseconds); }

void AcpiOsStall(UINT32 Microseconds) {
  /* Must not reschedule. Uncalibrated — swap for an rdtsc-based
     delay once you have a measured cycles-per-us constant. */
  volatile uint32_t i;
  uint32_t loops = Microseconds * 20;
  for (i = 0; i < loops; i++)
    asm volatile("nop");
}

ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *OutHandle) {
  _MUTEX *m = malloc(sizeof(_MUTEX));
  if (!m)
    return AE_NO_MEMORY;
  mutex_init(m);
  *OutHandle = (ACPI_MUTEX)m;
  return AE_OK;
}

void AcpiOsDeleteMutex(ACPI_MUTEX Handle) { free((_MUTEX *)Handle); }

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX Handle, UINT16 Timeout) {
  _MUTEX *m = (_MUTEX *)Handle;
  if (Timeout == 0)
    return mutex_trylock(m) ? AE_OK : AE_TIME;
  uint32_t ms = (Timeout == 0xFFFF) ? MUTEX_WAIT_FOREVER : Timeout;
  return mutex_lock(m, ms) ? AE_OK : AE_TIME;
}

void AcpiOsReleaseMutex(ACPI_MUTEX Handle) { mutex_unlock((_MUTEX *)Handle); }

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
                                  ACPI_SEMAPHORE *OutHandle) {
  _SEMAPHORE *s = malloc(sizeof(_SEMAPHORE));
  if (!s) {
    return AE_NO_MEMORY;
  }
  sem_init(s, InitialUnits, MaxUnits);
  *OutHandle = (ACPI_SEMAPHORE)s;
  return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
  free((_SEMAPHORE *)Handle);
  return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units,
                                UINT16 Timeout) {
  _SEMAPHORE *s = (_SEMAPHORE *)Handle;
  if (Timeout == 0)
    return sem_trywait(s, Units) ? AE_OK : AE_TIME;
  uint32_t ms = (Timeout == 0xFFFF) ? SEM_WAIT_FOREVER : Timeout;
  return sem_wait(s, Units, ms) ? AE_OK : AE_TIME;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
  sem_signal((_SEMAPHORE *)Handle, Units);
  return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) {
  _SPINLOCK *lock = malloc(sizeof(_SPINLOCK));
  if (!lock)
    return AE_NO_MEMORY;
  spinlock_init(lock);
  *OutHandle = (ACPI_SPINLOCK)lock;
  return AE_OK;
}

void AcpiOsDeleteLock(ACPI_HANDLE Handle) { free((_SPINLOCK *)Handle); }

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
  return (ACPI_CPU_FLAGS)spinlock_acquire((_SPINLOCK *)Handle);
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags) {
  spinlock_release((_SPINLOCK *)Handle, (uint32_t)Flags);
}

ACPI_STATUS AcpiOsAcquireGlobalLock(UINT32 *lock) {
  (void)lock;
  return AE_OK;
}

ACPI_STATUS AcpiOsReleaseGlobalLock(UINT32 *lock) {
  (void)lock;
  return AE_OK;
}