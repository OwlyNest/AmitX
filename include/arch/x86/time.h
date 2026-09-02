/*
 * include/arch/x86/time.h - Timekeeping interface
 * Author:   amity
 * Date:     Sat Jun 20 22:38:20 2026
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

#ifndef __ARCH_X86_TIME_H__
#define __ARCH_X86_TIME_H__

/* --- Includes ---*/
#include <arch/x86/interrupts.h>
#include <stdint.h>

/* --- Prototypes ---*/
int timer_callback(interrupt_frame_t *frame);
void sleep(uint32_t seconds);
void sleep_ms(uint32_t milliseconds);
void sleep_t(uint32_t ticks);

/* --- TSC-based high-resolution time --- */
void time_init_tsc(void);
uint64_t time_get_ntfs(void);         /* 100ns since 1601 (NT style) */
uint64_t time_get_unix(void);         /* seconds since 1970 */
uint64_t time_get_monotonic_ns(void); /* nanoseconds since boot */

/* --- High-resolution delays --- */
void delay_ns(uint64_t nanoseconds);
void delay_us(uint64_t microseconds);

#endif