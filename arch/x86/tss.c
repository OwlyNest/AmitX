/*
 * arch/x86/tss.c - [Enter description]
 * Author:   amity
 * Date:     Fri Jun 12 09:14:38 2026
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
extern void gdt_install();
/* --- Prototypes ---*/

/* --- Main ---*/

/* --- Functions ---*/

void gdt_init_tss(void) {
  struct gdt_entry *tss_desc = &gdt_start[5];
  uint32_t base = (uint32_t)tss_struct;
  uint32_t limit = (uint32_t)(tss_end - tss_struct - 1);
  tss_desc->limit_low = limit & 0xFFFF;
  tss_desc->base_low = base & 0xFFFF;
  tss_desc->base_mid = (base >> 16) & 0xFF;
  tss_desc->access = 0x89;
  tss_desc->granularity = ((limit >> 16) & 0x0F);
  tss_desc->base_high = (base >> 24) & 0xFF;
}

static int x86_gdt_init(void) {
  gdt_init_tss();
  gdt_install();
  tss_set_esp0(0x90000);
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