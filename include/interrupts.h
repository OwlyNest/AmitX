
#ifndef INTERRUPTS_H
#define INTERRUPTS_H

void isr0_handler();
void pic_remap();
void isr_handler(int interrupt_number);
void register_interrupt_handler(int n, void (*handler)());
#endif
