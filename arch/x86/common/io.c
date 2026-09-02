/*
 * arch/x86/common/io.c - Basic Input/Ouput System ;)
 * Author:   amity
 * Date:     Mon Aug 31 15:16:13 2026
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
#include <arch/x86/io.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/
BYTE inb(WORD port) {
  BYTE ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

VOID outb(WORD port, BYTE value) {
  __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

WORD inw(WORD port) {
  WORD ret;
  __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

VOID outw(WORD port, WORD value) {
  __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

DWORD inl(WORD port) {
  DWORD ret;
  __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

VOID outl(WORD port, DWORD val) {
  __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}