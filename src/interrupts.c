
#include "screen.h"
#include "io.h"
#include <stdint.h>

#define MAX_INTERRUPTS 256

void (*interrupt_handlers[MAX_INTERRUPTS])();

void register_interrupt_handler(int n, void (*handler)()) {
    interrupt_handlers[n] = handler;
}

// ISR entry point called from ASM stub
void isr_handler(int interrupt_number, uint32_t error_code) {

    if (interrupt_handlers[interrupt_number]) {
        interrupt_handlers[interrupt_number]();
    } else {
        puts("[unhandled interrupt] ");
        puthex(interrupt_number);
        puts(" (err=");
        puthex(error_code);
        puts(")");
        newline();
    }

    // Acknowledge PIC
    if (interrupt_number >= 40) {
        outb(0xA0, 0x20);  // Slave
    }
    if (interrupt_number >= 32) {
        outb(0x20, 0x20);  // Master
    }
}

// Special handler for divide-by-zero
void isr0_handler() {
    setcolor(0, 15);
    puts("[ERRNO-0]: Divide by zero\n");
    while (1) {}
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
