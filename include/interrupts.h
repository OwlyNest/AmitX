
#ifndef INTERRUPTS_H
#define INTERRUPTS_H

void divide_by_zero_handler();
void panic(const char* msg, uint32_t interrupt_number, uint32_t err);
void pic_remap();
void isr_handler(int int_no, uint32_t err);
void register_interrupt_handler(int n, void (*handler)());
#endif
