
#include "screen.h"
#include "io.h"
#include "amitx_consts.h"
#include <stdint.h>

#define MAX_INTERRUPTS 256

void (*interrupt_handlers[MAX_INTERRUPTS])();
void panic(const char* msg, uint32_t interrupt_number, uint32_t err);

void register_interrupt_handler(int n, void (*handler)()) {
    interrupt_handlers[n] = handler;
}

void pic_eoi(uint8_t irq) {
    if (irq >= 40) {
        outb(PORT_PIC_SLAVE_CMD, PIC_EOI);  // Slave PIC
    }
    outb(PORT_PIC_MASTER_CMD, PIC_EOI);      // Master PIC
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
    } else if (int_no == VECTOR_SYSCALL) {
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
    outb(PORT_PIC_MASTER_CMD, PIC_ICW1_INIT);
    outb(PORT_PIC_SLAVE_CMD, PIC_ICW1_INIT);

    outb(PORT_PIC_MASTER_DATA, 0x20);  // Master vector offset
    outb(PORT_PIC_SLAVE_DATA, 0x28);  // Slave vector offset

    outb(PORT_PIC_MASTER_DATA, PIC_ICW3_MASTER_SLAVE2);  // Tell master about slave at IRQ2
    outb(PORT_PIC_SLAVE_DATA, PIC_ICW3_SLAVE_ID2);  // Tell slave its cascade ID

    outb(PORT_PIC_MASTER_DATA, PIC_ICW4_8086);
    outb(PORT_PIC_SLAVE_DATA, PIC_ICW4_8086);

    outb(PORT_PIC_MASTER_DATA, 0x00);  // Unmask
    outb(PORT_PIC_SLAVE_DATA, 0x00);

    puts("Remap complete\n");
}

__attribute__((noreturn))
void panic(const char* msg, uint32_t int_no, uint32_t err) {
    setcolor(15, 4);  // white on red
    clear();
    
    move_cursor(10, 5);
    puts("  KERNEL PANIC  ");
    move_cursor(10, 7);
    puts(msg);
    move_cursor(10, 9);
    puts("INT: "); puthex(int_no);
    puts("  ERR: "); puthex(err);
    move_cursor(10, 11);
    puts("System halted.");
    
    asm("cli");
    for (;;) asm("hlt");
}
