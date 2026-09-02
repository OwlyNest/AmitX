/*
 * arch/x86/32/panic.c - [Enter description]
 * Author:   amity
 * Date:     Mon Aug 31 14:30:58 2026
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
#include <mm/paging.h>
#include <screen/printk.h>
#include <screen/screen.h>
#include <stdint.h>

/* --- Functions --- */
void arch_dump_panic_regs(void) {
  /* Not used directly — panic() does inline asm below */
}

void arch_dump_panic_frame_regs(interrupt_frame_t *frame) {
  (void)frame;
  /* Not used — panic_frame() accesses frame directly */
}

__attribute__((noreturn)) void panic(const char *msg, uint32_t int_no,
                                     uint32_t err) {
  uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp;

  __asm__ __volatile__("movl %%eax, %0\n\t"
                       "movl %%ebx, %1\n\t"
                       "movl %%ecx, %2\n\t"
                       "movl %%edx, %3\n\t"
                       "movl %%esi, %4\n\t"
                       "movl %%edi, %5\n\t"
                       "movl %%ebp, %6\n\t"
                       "movl %%esp, %7\n\t"
                       : "=m"(eax), "=m"(ebx), "=m"(ecx), "=m"(edx), "=m"(esi),
                         "=m"(edi), "=m"(ebp), "=m"(esp));

  setcolor(15, 4);
  clear();

  move_cursor(10, 5);
  printk("KERNEL PANIC\n\n");
  move_cursor(10, 7);
  printk(msg);
  move_cursor(10, 9);
  printk("INT: %x  ERR: %x", int_no, err);

  move_cursor(10, 11);
  printk("EAX: %x EBX: %x ECX: %x EDX: %x", eax, ebx, ecx, edx);
  move_cursor(10, 12);
  printk("ESI: %x EDI: %x EBP: %x ESP: %x", esi, edi, ebp, esp);

  move_cursor(10, 14);
  printk("Halting...");

  for (;;) {
    __asm__ volatile("hlt");
  }
}

__attribute__((noreturn)) void panic_frame(interrupt_frame_t *frame,
                                           const char *msg) {
  __asm__ volatile("cli");

  setcolor(15, 4);
  clear();

  move_cursor(10, 5);
  printk("KERNEL PANIC:\n");
  move_cursor(10, 7);
  printk("%s\n", msg);
  move_cursor(10, 9);
  printk("INT: 0x%02x  ERR: 0x%08x\n", frame->err_no, frame->err_code);

  move_cursor(10, 11);
  printk("EAX: %08x  EBX: %08x  ECX: %08x  EDX: %08x\n", frame->eax, frame->ebx,
         frame->ecx, frame->edx);
  move_cursor(10, 12);
  printk("ESI: %08x  EDI: %08x  EBP: %08x  ESP: %08x\n", frame->esi, frame->edi,
         frame->ebp, frame->esp);

  if (frame->err_no == 14) {
    uint32_t cr2;
    __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
    move_cursor(10, 13);
    printk("CR2=%08x\n", cr2);

    uint32_t cr3;
    __asm__("mov %%cr3,%0\n" : "=r"(cr3));
    move_cursor(10, 14);
    printk("CR3=%08x\n", cr3);
    move_cursor(10, 15);
    printk("CR2=%08x\n", cr2);
    move_cursor(10, 16);
    printk("virt_to_phys(CR2 page)=%08x\n", virt_to_phys(cr2 & ~0xFFF));
  }

  for (;;) {
    __asm__ volatile("hlt");
  }
}