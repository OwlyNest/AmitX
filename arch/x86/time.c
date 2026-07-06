/*
	* arch/x86/time.c - Timekeeping and sleep functions
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
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include <arch/x86/interrupts.h>
#include <arch/x86/time.h>
#include <arch/x86/timer.h>
#include <stdint.h>

#include <arch/x86/scheduler.h>
#include <arch/x86/task.h>
#include <screen/printk.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
volatile uint32_t tick_count = 0;
extern int menu;
/* --- Prototypes ---*/

/* --- Functions ---*/

/* ==========================================================================
 * PIT IRQ0 handler — increments global tick counter
 * ======================================================================= */

void timer_callback(interrupt_frame_t *frame) {
    (void)frame;
    tick_count++;
}


/* ==========================================================================
 * Sleep for N seconds
 * ======================================================================= */
void sleep(uint32_t seconds) {
    if (seconds == 0)
        return;

    uint32_t start = tick_count;
    uint32_t ticks = seconds * 100;

    while ((tick_count - start) < ticks)
        __asm__ __volatile__("hlt");
}

/* ==========================================================================
 * Sleep for N milliseconds (rounds up to nearest tick)
 * ======================================================================= */
void sleep_ms(uint32_t milliseconds) {
    if (milliseconds == 0)
        return;

    uint32_t start = tick_count;
    uint32_t ticks = (milliseconds + 9) / 10;

    while ((tick_count - start) < ticks)
        __asm__ __volatile__("hlt");
}

/* ==========================================================================
 * Sleep for N timer ticks
 * ======================================================================= */
void sleep_t(uint32_t ticks) {
    if (ticks == 0)
        return;

    uint32_t start = tick_count;

    while ((tick_count - start) < ticks)
        __asm__ __volatile__("hlt");
}