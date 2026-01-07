
#include "screen.h"
#include "io.h"
#include <stdint.h>

#define MAX_INTERRUPTS 256

void (*interrupt_handlers[MAX_INTERRUPTS])();
void panic(const char* msg, uint32_t interrupt_number, uint32_t err);
void register_interrupt_handler(int n, void (*handler)()) {
    interrupt_handlers[n] = handler;
}

void pic_eoi(uint8_t irq) {
    if (irq >= 40) {
        outb(0xA0, 0x20);  // Slave PIC
    }
    outb(0x20, 0x20);      // Master PIC
}

void isr_handler(int int_no, uint32_t err) {
    if (int_no < 32) {
        if (interrupt_handlers[int_no])
            interrupt_handlers[int_no](int_no, err);
        else
            panic("Unhandled CPU exception", int_no, err);
    } else if (int_no < 48) {
        if (interrupt_handlers[int_no])
            interrupt_handlers[int_no]();
        pic_eoi(int_no);
    } else if (int_no == 128) {
        // syscall handled elsewhere
    } else {
        panic("Unhandled software interrupt", int_no, err);
    }
}


void divide_by_zero_handler(uint32_t interrupt_number, uint32_t err) {
    panic("Divide by zero", interrupt_number, err);
}

// PIC remapping stays unchanged
void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    outb(0x21, 0x20);  // Master vector offset
    outb(0xA1, 0x28);  // Slave vector offset

    outb(0x21, 0x04);  // Tell master about slave at IRQ2
    outb(0xA1, 0x02);  // Tell slave its cascade ID

    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, 0x00);  // Unmask
    outb(0xA1, 0x00);

    puts("Remap complete\n");
}

__attribute__((noreturn))
void panic(const char* msg, uint32_t interrupt_number, uint32_t err) {
    setcolor(15, 4);
    puts("KERNEL PANIC\n");
    puts(msg);
    puts("\nINT: ");
    puthex(interrupt_number);
    puts(" ERR: ");
    puthex(err);
    newline();
    asm("cli");
    for (;;) asm("hlt");
}
