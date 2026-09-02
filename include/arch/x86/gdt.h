/*
 * arch/x86/gdt.h - Global Descriptor Table
 * Author:   amity
 * Date:     Fri Jun 12 08:44:29 2026
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
#ifndef __ARCH_X86_GDT_H__
#define __ARCH_X86_GDT_H__

#define SEL_KCODE 0x08
#define SEL_KDATA 0x10
#define SEL_UCODE 0x18
#define SEL_UDATA 0x20
#define SEL_TSS 0x28

#if ARCH_X86_64
#define SEL_TSS_HI 0x30
#endif

/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
struct gdt_entry {
  WORD limit_low;
  WORD base_low;
  BYTE base_mid;
  BYTE access;
  BYTE granularity;
  BYTE base_high;
} __attribute__((packed));

#if ARCH_X86_64
struct gdt_entry_64 {
  DWORD base_upper;
  DWORD reserved;
} __attribute__((packed));
#endif

/* --- Globals ---*/
extern struct gdt_entry gdt_start[];
#if ARCH_X86_64
extern struct gdt_entry_64 gdt_start_64[];
#endif
extern uint8_t tss_struct[];
extern uint8_t tss_end[];

/* --- Prototypes ---*/
void gdt_install(void);
void gdt_init_tss(void);

#if ARCH_X86_64
void tss_set_rsp0(uint64_t rsp);
void usermode_jump(uint64_t rip, uint64_t rsp);
#else
void tss_set_esp0(uint32_t esp);
void usermode_jump(uint32_t eip, uint32_t esp);
#endif
#endif