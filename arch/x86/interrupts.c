#include <screen/screen.h>
#include <arch/x86/io.h>
#include <screen/printk.h>
#include <hw/acpi.h>
#include <arch/x86/interrupts.h>
#include <arch/x86/time.h>
#include <internal/amitx_consts.h>
#include <mm/paging.h>
#include <stdint.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>

#define MAX_INTERRUPTS     256
#define MAX_IRQ_HANDLERS   4

static irq_handler_t interrupt_handlers[MAX_INTERRUPTS][MAX_IRQ_HANDLERS];

extern volatile uint32_t tick_count;
extern void syscall_dispatch(interrupt_frame_t *frame);

static const char* exception_names[32] = {
    "Divide Error", "Debug", "NMI", "Breakpoint", "Overflow",
    "BOUND Range", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS",
    "Segment Not Present", "Stack Segment Fault", "General Protection Fault",
    "Page Fault", "Reserved", "x87 FP Exception", "Alignment Check",
    "Machine Check", "SIMD Floating-Point Exception", "Virtualization Exception",
    "Control Protection Exception", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved"
};

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
/* Exceptions: we only use slot [0]                                   */
/* ------------------------------------------------------------------ */
int exception_handler(interrupt_frame_t *frame) {
    panic_frame(frame, exception_names[frame->err_no]);
    return 0; /* never reached */
}

void register_exception_handlers(void) {
    for (int i = 0; i < 32; i++) {
        interrupt_handlers[i][0] = exception_handler;
    }
    interrupt_handlers[8][0] = NULL; /* double fault uses task gate */
}

/* ------------------------------------------------------------------ */
/* PIC helpers                                                        */
/* ------------------------------------------------------------------ */
void pic_set_irq_level_triggered(uint8_t irq) {
    uint16_t port;
    uint8_t mask;

    if (irq == 0 || irq == 2 || irq == 8 || irq == 13) {
        /* These are system-reserved / edge-only */
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
    uint32_t int_no = frame->err_no;
    uint32_t err    = frame->err_code;
    int handled = 0;

    if (int_no < 32) {
        /* CPU exception */
        if (interrupt_handlers[int_no][0]) {
            interrupt_handlers[int_no][0](frame);
        } else {
            panic_frame(frame, exception_names[int_no]);
        }
    } else if (int_no < 48) {
        /* Hardware IRQ (0-15) */
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
            /*
             * Nobody claimed it.  Send EOI anyway so we don't
             * deadlock, but warn.  If a device is still asserting
             * the line we'll immediately re-enter, but at least
             * the system doesn't freeze.
             */
            printk("Unhandled IRQ%u (spurious?)\n", irq);
            pic_eoi(irq);
        }
    } else if (int_no == VECTOR_SYSCALL) {
        syscall_dispatch(frame);
    } else {
        panic("Unhandled software interrupt", int_no, err);
    }
}

/* ... pic_unmask_irq / pic_mask_irq stay exactly the same ... */
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

/* ... PIC remap stays the same ... */
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
    elcr_slave |= (1 << 1);   /* IRQ9 = level-triggered */
    outb(0x4D1, elcr_slave);

    printk("PIC remapped, all IRQs masked\n");
    return 0;
}

kscope_node_t x86_pic_node = {
    .name = "x86-pic",
    .id = 0x0002,
    .class = KSCOPE_CLASS_CORE,
    .subclass = KSCOPE_SUBCLASS_CORE_PIC,
    .requires = (kscope_node_t*[]){ &x86_gdt_node },
    .require_count = 1,
    .provides = (const char*[]){"cpu.pic", "irq.controller"},
    .provide_count = 2,
    .init = pic_remap,
};


__attribute__((noreturn))
void panic(const char* msg, uint32_t int_no, uint32_t err) {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp;

    __asm__ __volatile__ (
        "movl %%eax, %0\n\t"
        "movl %%ebx, %1\n\t"
        "movl %%ecx, %2\n\t"
        "movl %%edx, %3\n\t"
        "movl %%esi, %4\n\t"
        "movl %%edi, %5\n\t"
        "movl %%ebp, %6\n\t"
        "movl %%esp, %7\n\t"
        : "=m"(eax), "=m"(ebx), "=m"(ecx), "=m"(edx), "=m"(esi), "=m"(edi), "=m"(ebp), "=m"(esp)
    );

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
        __asm__ volatile ("hlt");
    }
}

__attribute__((noreturn))
void panic_frame(interrupt_frame_t *frame, const char *msg) {
    __asm__ volatile ("cli");

    setcolor(15, 4);
    clear();

    move_cursor(10, 5);
    printk("KERNEL PANIC:\n");
    move_cursor(10, 7);
    printk("%s\n", msg);
    move_cursor(10, 9);
    printk("INT: 0x%02x  ERR: 0x%08x\n", frame->err_no, frame->err_code);

    move_cursor(10, 11);
    printk("EAX: %08x  EBX: %08x  ECX: %08x  EDX: %08x\n",
           frame->eax, frame->ebx, frame->ecx, frame->edx);
    move_cursor(10, 12);
    printk("ESI: %08x  EDI: %08x  EBP: %08x  ESP: %08x\n",
           frame->esi, frame->edi, frame->ebp, frame->esp);
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
        __asm__ volatile ("hlt");
    }

}