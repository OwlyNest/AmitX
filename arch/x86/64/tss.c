/*
 * arch/x86/64/tss.c - Task State Segment (64-bit)
 * Author:   amity
 * Date:     Mon Aug 31 12:42:23 2026
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
#include <arch/x86/gdt.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern VOID gdt_install(VOID);

/* --- Prototypes ---*/

/* --- Functions ---*/
void gdt_init_tss(void) {
  struct gdt_entry *tss_low = &gdt_start[5];
  struct gdt_entry_64 *tss_high = &gdt_start_64[0];
  ULONG_PTR base = (ULONG_PTR)tss_struct;
  DWORD limit = (DWORD)(tss_end - tss_struct - 1);

  /* Low 32 bits of base in first slot (0x28) */
  tss_low->limit_low = limit & 0xFFFF;
  tss_low->base_low = base & 0xFFFF;
  tss_low->base_mid = (base >> 16) & 0xFF;
  tss_low->access = 0x89;      /* present, ring 0, available TSS */
  tss_low->granularity = 0x00; /* 64-bit TSS: G=0, limit high=0 */
  tss_low->base_high = (base >> 24) & 0xFF;

  /* High 32 bits of base in second slot (0x30) */
  tss_high->base_upper = (base >> 32) & 0xFFFFFFFF;
  tss_high->reserved = 0;
}

static int x86_gdt_init(void) {
  gdt_init_tss();
  gdt_install();
  tss_set_rsp0(0x90000);
  return 0;
}

static kscope_node_t *x86_gdt_requires[] = {&paging_node};

static const char *x86_gdt_provides[] = {"cpu.gdt", "cpu.tss", "cpu.segments"};

kscope_node_t x86_gdt_node = {
    .name = "x86-gdt",
    .id = 0x0001,
    .class = KSCOPE_CLASS_CORE,
    .subclass = KSCOPE_SUBCLASS_CORE_GDT,
    .requires = x86_gdt_requires,
    .require_count = 1,
    .provides = x86_gdt_provides,
    .provide_count = 3,
    .init = x86_gdt_init,
};