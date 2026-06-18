
#ifndef TIMER_H
#define TIMER_H

#include <arch/x86/interrupts.h>
#include <stdint.h>

void init_timer(uint32_t frequency);
void timer_callback(interrupt_frame_t *frame);

#endif
