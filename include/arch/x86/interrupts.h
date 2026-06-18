
#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

typedef struct {
    uint32_t err_no;
    uint32_t err_code;

    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} interrupt_frame_t;

typedef void (*irq_handler_t)(interrupt_frame_t *frame);

void divide_by_zero_handler();
void panic(const char* msg, uint32_t interrupt_number, uint32_t err);
void pic_unmask_irq(uint8_t irq);
void isr_handler(interrupt_frame_t *frame);
void register_interrupt_handler(int n, irq_handler_t handler);

__attribute__((noreturn))
void panic_frame(interrupt_frame_t *frame, const char *msg);
#endif