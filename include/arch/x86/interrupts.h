/*
 * include/arch/x86/interrupts.h - [Enter description]
 * Author:   amity
 * Date:     Mon Aug 31 12:04:10 2026
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
#ifndef __ARCH_X86_INTERRUPTS_H__
#define __ARCH_X86_INTERRUPTS_H__
/* --- Includes ---*/
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/
#if ARCH_X86_64

typedef struct {
  /* Error info (pushed by ISR stub or CPU) */
  uint64_t err_no;
  uint64_t err_code;

  /* General purpose registers */
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rbx;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rax;

  /* Segment selectors */
  uint64_t gs;
  uint64_t fs;

  /* Interrupt return state */
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
} interrupt_frame_t;

#else

typedef struct {
  uint32_t err_no;
  uint32_t err_code;

  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
} interrupt_frame_t;

#endif

/* Return 1 if you handled this interrupt, 0 if it came from another device */
typedef int (*irq_handler_t)(interrupt_frame_t *frame);
/* --- Globals ---*/

/* --- Prototypes ---*/
int exception_handler(interrupt_frame_t *frame);
void panic(const char *msg, uint32_t interrupt_number, uint32_t err);
void pic_unmask_irq(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_set_irq_level_triggered(uint8_t irq);
void isr_handler(interrupt_frame_t *frame);
void register_interrupt_handler(int n, irq_handler_t handler);

__attribute__((noreturn)) void panic_frame(interrupt_frame_t *frame,
                                           const char *msg);

#endif