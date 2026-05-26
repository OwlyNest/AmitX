
#ifndef IDT_H
#define IDT_H
#include <stdint.h>

void idt_install();
void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags);

struct IDTEntry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct IDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#endif
