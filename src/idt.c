
#include "idt.h"
#include "interrupts.h"
#include <stdint.h>

extern void isr0();
extern void isr32();
extern void isr33();
extern void isr44();
extern void isr128();
extern void load_idt(uint32_t);

#define IDT_ENTRIES 256
static struct IDTEntry idt[IDT_ENTRIES];
static struct IDTPointer idt_ptr;

void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;

    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}
void idt_install() {
    idt_ptr.limit = sizeof(struct IDTEntry) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uint32_t)&idt;

    idt_set_gate(0,   (uint32_t)isr0,   0x08, 0x8E);
    idt_set_gate(32,  (uint32_t)isr32,  0x08, 0x8E);
    idt_set_gate(33,  (uint32_t)isr33,  0x08, 0x8E);
    idt_set_gate(44,  (uint32_t)isr44,  0x08, 0x8E);
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0x8E);

    load_idt((uint32_t)&idt_ptr);
}
