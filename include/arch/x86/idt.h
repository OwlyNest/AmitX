/*
 * include/arch/x86/idt.h - Interrupt Descriptor Table
 * Author:   amity
 * Date:     Mon Aug 31 12:01:12 2026
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
#ifndef __ARCH_X86_IDT_H__
#define __ARCH_X86_IDT_H__
/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
#if ARCH_X86_64

struct IDTEntry {
  WORD base_lo;
  WORD sel;
  BYTE ist; /* IST index (bits 0-2), rest reserved */
  BYTE flags;
  WORD base_mid;
  DWORD base_hi;
  DWORD reserved; /* Must be zero */
} __attribute__((packed));

struct IDTPointer {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));
#else
struct IDTEntry {
  WORD base_lo;
  WORD sel;
  BYTE always0;
  BYTE flags;
  WORD base_hi;
} __attribute__((packed));

struct IDTPointer {
  WORD limit;
  DWORD base;
} __attribute__((packed));
#endif

/* --- Globals ---*/

/* --- Prototypes ---*/
void idt_set_gate(int num, ULONG_PTR base, WORD sel, BYTE flags);

#endif
