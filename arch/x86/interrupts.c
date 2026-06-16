#include <screen/screen.h>
#include <arch/x86/io.h>
#include <screen/printk.h>
#include <hw/acpi.h>
#include <arch/x86/interrupts.h>
#include <arch/x86/time.h>
#include <internal/amitx_consts.h>
#include <stdint.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>

#define MAX_INTERRUPTS 256

extern volatile uint32_t tick_count;

static const char* exception_names[32] = {
    "Divide Error",
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
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FP Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD FP Exception",
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
    "Reserved"
};

void (*interrupt_handlers[MAX_INTERRUPTS])();
void panic(const char* msg, uint32_t interrupt_number, uint32_t err);

void register_interrupt_handler(int n, void (*handler)()) {
    interrupt_handlers[n] = handler;
}

void exception_handler(uint32_t int_no, uint32_t err) {
    panic(exception_names[int_no], int_no, err);
}

void register_exception_handlers(void) {
    for (int i = 0; i < 32; i++) {
        interrupt_handlers[i] = exception_handler;
    }
}

void halt(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void pic_eoi(uint8_t irq) {
    if (irq >= 40) {
        outb(PORT_PIC_SLAVE_CMD, PIC_EOI);  // Slave PIC
    }
    outb(PORT_PIC_MASTER_CMD, PIC_EOI);      // Master PIC
}

void isr_handler(int int_no, uint32_t err) {
    if (int_no < 32) {
        if (interrupt_handlers[int_no]) {
            interrupt_handlers[int_no](int_no, err);
        } else {
            panic(exception_names[int_no], int_no, err);
        }
    } else if (int_no < 48) {
        if (interrupt_handlers[int_no])
            interrupt_handlers[int_no]();
        pic_eoi(int_no);
    } else if (int_no == VECTOR_SYSCALL) {
        // syscall handled elsewhere
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

void divide_by_zero_handler(uint32_t interrupt_number, uint32_t err) {
    panic("Divide by zero", interrupt_number, err);
}

// PIC remapping stays unchanged
static int pic_remap() {
    outb(PORT_PIC_MASTER_CMD, PIC_ICW1_INIT);
    outb(PORT_PIC_SLAVE_CMD, PIC_ICW1_INIT);

    outb(PORT_PIC_MASTER_DATA, 0x20);  // Master vector offset
    outb(PORT_PIC_SLAVE_DATA, 0x28);  // Slave vector offset

    outb(PORT_PIC_MASTER_DATA, PIC_ICW3_MASTER_SLAVE2);
    outb(PORT_PIC_SLAVE_DATA, PIC_ICW3_SLAVE_ID2);

    outb(PORT_PIC_MASTER_DATA, PIC_ICW4_8086);
    outb(PORT_PIC_SLAVE_DATA, PIC_ICW4_8086);

    /* Mask ALL interrupts initially — unmask selectively per driver */
    outb(PORT_PIC_MASTER_DATA, 0xFF);  // Mask all master IRQs
    outb(PORT_PIC_SLAVE_DATA, 0xFF);     // Mask all slave IRQs

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
    setcolor(15, 4);
    clear();

    move_cursor(10, 5);
    printk("KERNEL PANIC\n\n");
    move_cursor(10, 7);
    printk(msg);
    move_cursor(10, 9);
    printk("INT: %x", int_no);
    printk("  ERR: %x", err);
    move_cursor(10, 11);
    printk("Rebooting in 5 seconds...");

    asm volatile("sti");

    sleep(5);

    acpi_reboot();

    for (;;)
        __asm__ volatile ("hlt");
}