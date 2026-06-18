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

/*
    * 1) Stack trace in panic — walk EBP chain, print return addresses
    * 2) IRQ nesting / in_irq flag — prevent reentrant issues
    * 3) Mouse handler migration — you said you already did this
    * 4) Timer handler cleanup — remove timer_callback_wrapper indirection
    * 5) APIC planning — since you have MADT, sketch the migration
*/

#define MAX_INTERRUPTS 256

static irq_handler_t interrupt_handlers[MAX_INTERRUPTS];

extern volatile uint32_t tick_count;
extern void syscall_dispatch(interrupt_frame_t *frame);

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

void panic(const char* msg, uint32_t interrupt_number, uint32_t err);

void register_interrupt_handler(int n, irq_handler_t handler) {
    interrupt_handlers[n] = handler;
}

void exception_handler(interrupt_frame_t *frame) {
    panic_frame(frame, exception_names[frame->err_no]);
}

void register_exception_handlers(void) {
    for (int i = 0; i < 32; i++) {
        interrupt_handlers[i] = exception_handler;
    }
    interrupt_handlers[8] = NULL;
}

void halt(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void pic_eoi(uint8_t irq) {
    uint8_t isr;

    if (irq == 7) {
        outb(PORT_PIC_MASTER_CMD, 0x0B);
        isr = inb(PORT_PIC_MASTER_CMD);
        if (!(isr & 0x80)) {
            return;
        }
    }
    if (irq == 15) {
        outb(PORT_PIC_SLAVE_CMD, 0x0B);
        isr = inb(PORT_PIC_SLAVE_CMD);
        if (!(isr & 0x80)) {
            outb(PORT_PIC_MASTER_CMD, PIC_EOI);
            return;
        }
    }
    if (irq >= 8) {
        outb(PORT_PIC_SLAVE_CMD, PIC_EOI);
    }
    outb(PORT_PIC_MASTER_CMD, PIC_EOI);
}

void isr_handler(interrupt_frame_t *frame) {
    uint32_t int_no = frame->err_no;
    uint32_t err    = frame->err_code;
    if (int_no < 32) {
        if (interrupt_handlers[int_no]) {
            interrupt_handlers[int_no](frame);
        } else {
            panic_frame(frame, exception_names[int_no]);
        }
    } else if (int_no < 48) {
        if (interrupt_handlers[int_no])
            interrupt_handlers[int_no](frame);
        pic_eoi(int_no);
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

    for (;;)
        __asm__ volatile ("hlt");
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

    for (;;) {
        __asm__ volatile ("hlt");
    }

}