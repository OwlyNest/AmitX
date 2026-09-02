/*
 * arch/x86/32/idt.c - Interrupt Descriptor Table (32-bit)
 * Author:   amity
 * Date:     Mon Aug 31 14:17:53 2026
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
#define IDT_ENTRIES 256
/* --- Includes ---*/
#include <arch/x86/idt.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <internal/phonon_consts.h>
#include <stdint.h>

/* --- ISR stubs from isr.S --- */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void isr32(void);
extern void isr33(void);
extern void isr34(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr38(void);
extern void isr39(void);
extern void isr40(void);
extern void isr41(void);
extern void isr42(void);
extern void isr43(void);
extern void isr44(void);
extern void isr45(void);
extern void isr46(void);
extern void isr47(void);
extern void isr128(void);

/* --- Globals --- */

extern void load_idt(DWORD);

static struct IDTEntry idt[IDT_ENTRIES];
static struct IDTPointer idt_ptr;

static void (*const exception_isrs[])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,  isr8,  isr9,  isr10,
    isr11, isr12, isr13, isr14, isr15, isr16, isr17, isr18, isr19, isr20, isr21,
    isr22, isr23, isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31};

/* --- Functions --- */

/* ==========================================================================
 * idt_set_gate()
 * ======================================================================= */
void idt_set_gate(int num, ULONG_PTR base, WORD sel, BYTE flags) {
  idt[num].base_lo = base & 0xFFFF;
  idt[num].base_hi = (base >> 16) & 0xFFFF;

  idt[num].sel = sel;
  idt[num].always0 = 0;
  idt[num].flags = flags;
}

/* ==========================================================================
 * idt_install()
 * ======================================================================= */
static int idt_install(void) {
  idt_ptr.limit = sizeof(struct IDTEntry) * IDT_ENTRIES - 1;
  idt_ptr.base = (DWORD)&idt;

  for (int i = 0; i < 32; i++) {
    idt_set_gate(i, (ULONG_PTR)exception_isrs[i], GDT_SEL_KERNEL_CODE,
                 IDT_FLAGS_KERNEL);
  }

  idt_set_gate(32, (ULONG_PTR)isr32, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(33, (ULONG_PTR)isr33, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(34, (ULONG_PTR)isr34, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(35, (ULONG_PTR)isr35, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(36, (ULONG_PTR)isr36, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(37, (ULONG_PTR)isr37, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(38, (ULONG_PTR)isr38, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(39, (ULONG_PTR)isr39, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(40, (ULONG_PTR)isr40, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(41, (ULONG_PTR)isr41, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(42, (ULONG_PTR)isr42, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(43, (ULONG_PTR)isr43, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(44, (ULONG_PTR)isr44, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(45, (ULONG_PTR)isr45, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(46, (ULONG_PTR)isr46, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(47, (ULONG_PTR)isr47, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
  idt_set_gate(128, (ULONG_PTR)isr128, GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);

  load_idt((DWORD)&idt_ptr);
  return 0;
}

/* --- KScope node --- */
static kscope_node_t *x86_idt_requires[] = {&x86_gdt_node, &x86_pic_node,
                                            &paging_node};

static const char *x86_idt_provides[] = {"cpu.interrupts", "cpu.irq"};

kscope_node_t x86_idt_node = {
    .name = "x86-idt",
    .id = 0x0003,
    .class = KSCOPE_CLASS_CORE,
    .subclass = KSCOPE_SUBCLASS_CORE_IDT,
    .requires = x86_idt_requires,
    .require_count = 3,
    .provides = x86_idt_provides,
    .provide_count = 2,
    .init = idt_install,
};