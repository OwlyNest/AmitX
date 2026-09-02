/*
 * arch/x86/common/interrupts.c - [Enter description]
 * Author:   amity
 * Date:     Mon Aug 31 14:34:13 2026
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
#define MAX_INTERRUPTS 256
#define MAX_IRQ_HANDLERS 4

/* --- Includes ---*/
#include <arch/x86/interrupts.h>
#include <arch/x86/io.h>
#include <arch/x86/time.h>
#include <hw/acpi.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <internal/phonon_consts.h>
#include <mm/paging.h>
#include <screen/printk.h>
#include <screen/screen.h>
#include <stdint.h>

static irq_handler_t interrupt_handlers[MAX_INTERRUPTS][MAX_IRQ_HANDLERS];

extern volatile uint32_t tick_count;
extern void syscall_dispatch(interrupt_frame_t *frame);

static const char *exception_names[32] = {"Divide Error",
                                          "Debug",
                                          "NMI",
                                          "Breakpoint",
                                          "Overflow",
                                          "BOUND Range",
                                          "Invalid Opcode",
                                          "Device Not Available",
                                          "Double Fault",
                                          "Coprocessor Segment Overrun",
                                          "Invalid TSS",
                                          "Segment Not Present",
                                          "Stack Segment Fault",
                                          "General Protection Fault",
                                          "Page Fault",
                                          "Reserved",
                                          "x87 FP Exception",
                                          "Alignment Check",
                                          "Machine Check",
                                          "SIMD Floating-Point Exception",
                                          "Virtualization Exception",
                                          "Control Protection Exception",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved"};

/* --- Arch-specific helpers (declared here, defined in 32/64 subdirs) --- */
extern void arch_dump_panic_regs(void);
extern void arch_dump_panic_frame_regs(interrupt_frame_t *frame);

/* ------------------------------------------------------------------ */
/* Register a handler on a shared vector.                             */
/* ------------------------------------------------------------------ */
void register_interrupt_handler(int n, irq_handler_t handler) {
  for (int i = 0; i < MAX_IRQ_HANDLERS; i++) {
    if (interrupt_handlers[n][i] == NULL) {
      interrupt_handlers[n][i] = handler;
      return;
    }
  }
  printk("WARN: no free handler slot for vector %d\n", n);
}

/* ------------------------------------------------------------------ */
/* Exceptions                                                         */
/* ------------------------------------------------------------------ */
int exception_handler(interrupt_frame_t *frame) {
  panic_frame(frame, exception_names[frame->err_no]);
  return 0; /* never reached */
}

void register_exception_handlers(void) {
  for (int i = 0; i < 32; i++) {
    interrupt_handlers[i][0] = exception_handler;
  }
  interrupt_handlers[8][0] = NULL; /* double fault uses task gate / IST */
}

/* ------------------------------------------------------------------ */
/* PIC helpers                                                        */
/* ------------------------------------------------------------------ */
void pic_set_irq_level_triggered(uint8_t irq) {
  uint16_t port;
  uint8_t mask;

  if (irq == 0 || irq == 2 || irq == 8 || irq == 13) {
    return;
  }

  if (irq < 8) {
    port = 0x4D0;
    mask = (1 << irq);
  } else {
    port = 0x4D1;
    mask = (1 << (irq - 8));
  }

  uint8_t val = inb(port);
  val |= mask;
  outb(port, val);
}

void pic_eoi(uint8_t irq) {
  uint8_t isr;

  if (irq == 7) {
    outb(PORT_PIC_MASTER_CMD, 0x0B);
    isr = inb(PORT_PIC_MASTER_CMD);
    if (!(isr & 0x80)) {
      return; /* spurious */
    }
  }
  if (irq == 15) {
    outb(PORT_PIC_SLAVE_CMD, 0x0B);
    isr = inb(PORT_PIC_SLAVE_CMD);
    if (!(isr & 0x80)) {
      outb(PORT_PIC_MASTER_CMD, PIC_EOI);
      return; /* spurious */
    }
  }
  if (irq >= 8) {
    outb(PORT_PIC_SLAVE_CMD, PIC_EOI);
  }
  outb(PORT_PIC_MASTER_CMD, PIC_EOI);
}

/* ------------------------------------------------------------------ */
/* Main dispatcher called from isr.S                                  */
/* ------------------------------------------------------------------ */
void isr_handler(interrupt_frame_t *frame) {
  uint32_t int_no = (uint32_t)frame->err_no;
  uint32_t err = (uint32_t)frame->err_code;
  int handled = 0;

  if (int_no < 32) {
    if (interrupt_handlers[int_no][0]) {
      interrupt_handlers[int_no][0](frame);
    } else {
      panic_frame(frame, exception_names[int_no]);
    }
  } else if (int_no < 48) {
    uint32_t irq = int_no - 32;

    for (int i = 0; i < MAX_IRQ_HANDLERS; i++) {
      if (interrupt_handlers[int_no][i]) {
        if (interrupt_handlers[int_no][i](frame)) {
          handled = 1;
        }
      }
    }

    if (handled) {
      pic_eoi(irq);
    } else {
      printk("Unhandled IRQ%u (spurious?)\n", irq);
      pic_eoi(irq);
    }
  } else if (int_no == VECTOR_SYSCALL) {
    syscall_dispatch(frame);
  } else {
    panic("Unhandled software interrupt", int_no, err);
  }
}

void pic_unmask_irq(uint8_t irq) {
  uint16_t port;
  uint8_t value;

  if (irq < 8) {
    port = PORT_PIC_MASTER_DATA;
  } else {
    port = PORT_PIC_SLAVE_DATA;
    irq -= 8;
  }

  value = inb(port) & ~(1 << irq);
  outb(port, value);
}

void pic_mask_irq(uint8_t irq) {
  uint16_t port;
  uint8_t value;

  if (irq < 8) {
    port = PORT_PIC_MASTER_DATA;
  } else {
    port = PORT_PIC_SLAVE_DATA;
    irq -= 8;
  }
  value = inb(port) | (1 << irq);
  outb(port, value);
}

static int pic_remap(void) {
  outb(PORT_PIC_MASTER_CMD, PIC_ICW1_INIT);
  outb(PORT_PIC_SLAVE_CMD, PIC_ICW1_INIT);

  outb(PORT_PIC_MASTER_DATA, 0x20);
  outb(PORT_PIC_SLAVE_DATA, 0x28);

  outb(PORT_PIC_MASTER_DATA, PIC_ICW3_MASTER_SLAVE2);
  outb(PORT_PIC_SLAVE_DATA, PIC_ICW3_SLAVE_ID2);

  outb(PORT_PIC_MASTER_DATA, PIC_ICW4_8086);
  outb(PORT_PIC_SLAVE_DATA, PIC_ICW4_8086);

  outb(PORT_PIC_MASTER_DATA, 0xFF);
  outb(PORT_PIC_SLAVE_DATA, 0xFF);

  uint8_t elcr_slave = inb(0x4D1);
  elcr_slave |= (1 << 1);
  outb(0x4D1, elcr_slave);

  printk("PIC remapped, all IRQs masked\n");
  return 0;
}

static kscope_node_t *x86_pic_requires[] = {&x86_gdt_node};

static const char *x86_pic_provides[] = {"cpu.pic", "irq.controller"};

kscope_node_t x86_pic_node = {
    .name = "x86-pic",
    .id = 0x0002,
    .class = KSCOPE_CLASS_CORE,
    .subclass = KSCOPE_SUBCLASS_CORE_PIC,
    .requires = x86_pic_requires,
    .require_count = 1,
    .provides = x86_pic_provides,
    .provide_count = 2,
    .init = pic_remap,
};