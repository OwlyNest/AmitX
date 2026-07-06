
#ifndef __ARCH_X86_TIMER_H__
#define __ARCH_X86_TIMER_H__

#include <arch/x86/interrupts.h>
#include <stdint.h>

void init_timer(uint32_t frequency);
void timer_callback(interrupt_frame_t *frame);

#endif
