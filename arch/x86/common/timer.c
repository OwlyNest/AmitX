/*
 * arch/x86/common/timer.c - PIT timer driver
 * Author:   amity
 * Date:     Sat Jun 20 22:34:16 2026
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
#include <arch/x86/interrupts.h>
#include <arch/x86/io.h>
#include <arch/x86/timer.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <internal/phonon_consts.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* Handler signature must match interrupt_frame_t* */
static int (*timer_handler)(interrupt_frame_t *) = NULL;

/* --- Prototypes ---*/

/* --- Functions ---*/
/* ==========================================================================
 * Initialize PIT channel 0 to the requested frequency.
 * Safe fallback if frequency is zero or too low.
 * ======================================================================= */
void init_timer(uint32_t frequency) {
  if (frequency == 0) {
    printk("[timer] Invalid frequency 0, using 100 Hz\n");
    frequency = 100;
  }

  uint32_t divisor = PIT_BASE_HZ / frequency;

  if (divisor > 0xFFFF) {
    printk("[timer] Frequency too low, using 18 Hz\n");
    divisor = PIT_BASE_HZ / 18;
  }

  /* Channel 0, lobyte/hibyte, mode 3 (square wave), binary */
  outb(PORT_PIT_CMD, PIT_MODE_SQUARE_WAVE);
  outb(PORT_PIT_CH0, divisor & 0xFF);
  outb(PORT_PIT_CH0, (divisor >> 8) & 0xFF);

  timer_handler = timer_callback;
  register_interrupt_handler(VECTOR_IRQ0, timer_callback);

  printk("[timer] PIT initialized at %u Hz (divisor %u)\n",
         PIT_BASE_HZ / divisor, divisor);

  pic_unmask_irq(0);
}

/* ==========================================================================
 * KScope init wrapper
 * ======================================================================= */
static int timer_kscope_init(void) {
  init_timer(100);
  return 0;
}

kscope_node_t pit_timer_node = {
    .name = "pit-timer",
    .id = 0x0004,
    .class = KSCOPE_CLASS_TIME,
    .subclass = KSCOPE_SUBCLASS_TIME_PIT,
    .requires = (kscope_node_t *[]){&x86_pic_node, &x86_idt_node},
    .require_count = 2,
    .provides = (const char *[]){"time.pit", "irq.0", "sched.timer"},
    .provide_count = 3,
    .init = timer_kscope_init};