/*
 * arch/x86/64/panic.c - [Enter description]
 * Author:   amity
 * Date:     Mon Aug 31 14:30:54 2026
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

__attribute__((noreturn)) void panic(const char *msg, uint32_t int_no,
                                     uint32_t err) {
  uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

  __asm__ __volatile__("movq %%rax, %0\n\t"
                       "movq %%rbx, %1\n\t"
                       "movq %%rcx, %2\n\t"
                       "movq %%rdx, %3\n\t"
                       "movq %%rsi, %4\n\t"
                       "movq %%rdi, %5\n\t"
                       "movq %%rbp, %6\n\t"
                       "movq %%rsp, %7\n\t"
                       "movq %%r8,  %8\n\t"
                       "movq %%r9,  %9\n\t"
                       "movq %%r10, %10\n\t"
                       "movq %%r11, %11\n\t"
                       "movq %%r12, %12\n\t"
                       "movq %%r13, %13\n\t"
                       "movq %%r14, %14\n\t"
                       "movq %%r15, %15\n\t"
                       : "=m"(rax), "=m"(rbx), "=m"(rcx), "=m"(rdx), "=m"(rsi),
                         "=m"(rdi), "=m"(rbp), "=m"(rsp), "=m"(r8), "=m"(r9),
                         "=m"(r10), "=m"(r11), "=m"(r12), "=m"(r13), "=m"(r14),
                         "=m"(r15));

  setcolor(15, 4);
  clear();

  move_cursor(10, 5);
  printk("KERNEL PANIC\n\n");
  move_cursor(10, 7);
  printk(msg);
  move_cursor(10, 9);
  printk("INT: %x  ERR: %x", int_no, err);

  move_cursor(10, 11);
  printk("RAX: %016llx  RBX: %016llx\n", rax, rbx);
  move_cursor(10, 12);
  printk("RCX: %016llx  RDX: %016llx\n", rcx, rdx);
  move_cursor(10, 13);
  printk("RSI: %016llx  RDI: %016llx\n", rsi, rdi);
  move_cursor(10, 14);
  printk("RBP: %016llx  RSP: %016llx\n", rbp, rsp);
  move_cursor(10, 15);
  printk("R8:  %016llx  R9:  %016llx\n", r8, r9);
  move_cursor(10, 16);
  printk("R10: %016llx  R11: %016llx\n", r10, r11);
  move_cursor(10, 17);
  printk("R12: %016llx  R13: %016llx\n", r12, r13);
  move_cursor(10, 18);
  printk("R14: %016llx  R15: %016llx\n", r14, r15);

  move_cursor(10, 20);
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
  printk("INT: 0x%02llx  ERR: 0x%016llx\n", frame->err_no, frame->err_code);

  move_cursor(10, 11);
  printk("RAX: %016llx  RBX: %016llx  RCX: %016llx  RDX: %016llx\n", frame->rax,
         frame->rbx, frame->rcx, frame->rdx);
  move_cursor(10, 12);
  printk("RSI: %016llx  RDI: %016llx  RBP: %016llx  RSP: %016llx\n", frame->rsi,
         frame->rdi, frame->rbp, frame->rsp);
  move_cursor(10, 13);
  printk("R8:  %016llx  R9:  %016llx  R10: %016llx  R11: %016llx\n", frame->r8,
         frame->r9, frame->r10, frame->r11);
  move_cursor(10, 14);
  printk("R12: %016llx  R13: %016llx  R14: %016llx  R15: %016llx\n", frame->r12,
         frame->r13, frame->r14, frame->r15);
  move_cursor(10, 15);
  printk("RIP: %016llx  CS:  %04llx  RFLAGS: %016llx\n", frame->rip, frame->cs,
         frame->rflags);
  move_cursor(10, 16);
  printk("SS:  %016llx  RSP: %016llx\n", frame->ss, frame->rsp);

  if (frame->err_no == 14) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
    move_cursor(10, 18);
    printk("CR2=%016llx\n", cr2);

    uint64_t cr3;
    __asm__("mov %%cr3,%0\n" : "=r"(cr3));
    move_cursor(10, 19);
    printk("CR3=%016llx\n", cr3);
    move_cursor(10, 20);
    printk("virt_to_phys(CR2 page)=%016llx\n", virt_to_phys(cr2 & ~0xFFF));
  }

  for (;;) {
    __asm__ volatile("hlt");
  }
}