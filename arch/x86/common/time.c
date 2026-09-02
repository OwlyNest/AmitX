/*
 * arch/x86/common/time.c - Timekeeping and sleep functions
 * Author:   amity
 * Date:     Sat Jun 20 22:37:53 2026
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
/* NT-style 100ns epoch: January 1, 1601 */
#define SHADOW_EPOCH_OFFSET_100NS 0x019DB1DED53E8000ULL

/* --- Includes ---*/
#include <arch/x86/cpuid.h>
#include <arch/x86/interrupts.h>
#include <arch/x86/scheduler.h>
#include <arch/x86/task.h>
#include <arch/x86/time.h>
#include <arch/x86/timer.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Globals ---*/
volatile uint32_t tick_count = 0;

/* TSC-based timekeeping */
static uint64_t tsc_frequency = 0; /* TSC counts per second */
static uint64_t tsc_base = 0;      /* TSC value at boot */

/* ==========================================================================
 * TSC read (RDTSC or RDTSCP)
 * ======================================================================= */
static inline uint64_t read_tsc(void) {
#if ARCH_X86_64
  uint32_t eax, edx;
  __asm__ __volatile__("rdtsc" : "=a"(eax), "=d"(edx));
  return ((uint64_t)edx << 32) | eax;
#else
  uint32_t eax, edx;
  __asm__ __volatile__("rdtsc" : "=a"(eax), "=d"(edx));
  return ((uint64_t)edx << 32) | eax;
#endif
}

#if ARCH_X86_64
static inline uint64_t read_tscp(uint32_t *aux) {
  uint32_t eax, edx;
  __asm__ __volatile__("rdtscp" : "=a"(eax), "=d"(edx), "=c"(*aux));
  return ((uint64_t)edx << 32) | eax;
}
#endif

/* ==========================================================================
 * TSC calibration
 *
 * We calibrate TSC against the PIT. Count TSC ticks over a known
 * PIT interval (e.g., 1/100th second = 10ms).
 * ======================================================================= */
static uint64_t calibrate_tsc(void) {
  uint64_t tsc_start, tsc_end;
  uint32_t start_tick;

  /* Wait for a tick boundary */
  start_tick = tick_count;
  while (tick_count == start_tick)
    __asm__ __volatile__("pause");

  tsc_start = read_tsc();
  start_tick = tick_count;

  /* Wait for 10 ticks (100ms at 100Hz) */
  while ((tick_count - start_tick) < 10)
    __asm__ __volatile__("pause");

  tsc_end = read_tsc();

  /* TSC counts per second = (tsc_end - tsc_start) * 10 */
  uint64_t delta = tsc_end - tsc_start;
  return delta * 10;
}

void time_init_tsc(void) {
  tsc_base = read_tsc();
  tsc_frequency = calibrate_tsc();

  printk("[time] TSC frequency: %llu Hz\n", tsc_frequency);
}

/* ==========================================================================
 * High-resolution time queries
 * ======================================================================= */

uint64_t time_get_shadow(void) {
  if (tsc_frequency == 0) {
    return 0; /* TSC not calibrated yet */
  }

  uint64_t tsc_now = read_tsc();
  uint64_t tsc_delta = tsc_now - tsc_base;

  /* Convert TSC ticks to 100ns intervals:
   *   100ns_count = tsc_delta * 10_000_000 / tsc_frequency
   *
   * To avoid 64-bit overflow on fast CPUs, we do:
   *   tsc_delta / tsc_frequency = seconds
   *   (tsc_delta % tsc_frequency) * 10_000_000 / tsc_frequency = fractional
   */
  uint64_t seconds = tsc_delta / tsc_frequency;
  uint64_t remainder = tsc_delta % tsc_frequency;
  uint64_t frac_100ns = (remainder * 10000000ULL) / tsc_frequency;

  uint64_t total_100ns = seconds * 10000000ULL + frac_100ns;

  return SHADOW_EPOCH_OFFSET_100NS + total_100ns;
}

/*
 * Get current time as Unix-style seconds since 1970.
 */
uint64_t time_get_unix(void) {
  uint64_t shadow = time_get_shadow();
  return (shadow / 10000000ULL) - 11644473600ULL;
}

/*
 * Get monotonic time in nanoseconds (since boot).
 * Useful for benchmarking, timeouts, etc.
 */
uint64_t time_get_monotonic_ns(void) {
  if (tsc_frequency == 0) {
    return (uint64_t)tick_count * 10000000ULL; /* Fallback: 10ms ticks */
  }

  uint64_t tsc_now = read_tsc();
  uint64_t tsc_delta = tsc_now - tsc_base;

  /* nanoseconds = tsc_delta * 1_000_000_000 / tsc_frequency */
  uint64_t seconds = tsc_delta / tsc_frequency;
  uint64_t remainder = tsc_delta % tsc_frequency;
  uint64_t frac_ns = (remainder * 1000000000ULL) / tsc_frequency;

  return seconds * 1000000000ULL + frac_ns;
}

/* ==========================================================================
 * PIT IRQ0 handler
 * ======================================================================= */
int timer_callback(interrupt_frame_t *frame) {
  (void)frame;
  tick_count++;
  scheduler_tick();
  return 1;
}

/* ==========================================================================
 * Sleep functions (coarse, PIT-based)
 * ======================================================================= */
void sleep(uint32_t seconds) {
  if (seconds == 0)
    return;

  uint32_t start = tick_count;
  uint32_t ticks = seconds * 100;

  while ((tick_count - start) < ticks)
    __asm__ __volatile__("hlt");
}

void sleep_ms(uint32_t milliseconds) {
  if (milliseconds == 0)
    return;

  uint32_t start = tick_count;
  uint32_t ticks = (milliseconds + 9) / 10;

  while ((tick_count - start) < ticks)
    __asm__ __volatile__("hlt");
}

void sleep_t(uint32_t ticks) {
  if (ticks == 0)
    return;

  uint32_t start = tick_count;

  while ((tick_count - start) < ticks)
    __asm__ __volatile__("hlt");
}

/* ==========================================================================
 * High-resolution spin delay (TSC-based, for device init, etc.)
 * ======================================================================= */
void delay_ns(uint64_t nanoseconds) {
  if (tsc_frequency == 0) {
    /* Fallback: busy-wait with PIT ticks (coarse) */
    uint32_t start = tick_count;
    uint32_t ticks = (uint32_t)((nanoseconds + 9999999) / 10000000);
    while ((tick_count - start) < ticks)
      __asm__ __volatile__("pause");
    return;
  }

  uint64_t tsc_start = read_tsc();
  uint64_t tsc_target = (nanoseconds * tsc_frequency) / 1000000000ULL;

  while ((read_tsc() - tsc_start) < tsc_target)
    __asm__ __volatile__("pause");
}

void delay_us(uint64_t microseconds) { delay_ns(microseconds * 1000); }