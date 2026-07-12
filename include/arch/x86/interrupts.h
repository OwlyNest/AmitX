#ifndef __ARCH_X86_INTERRUPTS_H__
#define __ARCH_X86_INTERRUPTS_H__

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

/* Return 1 if you handled this interrupt, 0 if it came from another device */
typedef int (*irq_handler_t)(interrupt_frame_t *frame);

int exception_handler(interrupt_frame_t *frame);
void panic(const char* msg, uint32_t interrupt_number, uint32_t err);
void pic_unmask_irq(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_set_irq_level_triggered(uint8_t irq);
void isr_handler(interrupt_frame_t *frame);
void register_interrupt_handler(int n, irq_handler_t handler);

__attribute__((noreturn))
void panic_frame(interrupt_frame_t *frame, const char *msg);

#endif