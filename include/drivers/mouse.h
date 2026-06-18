#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <arch/x86/interrupts.h>

void mouse_handler(interrupt_frame_t *frame);
void get_mouse_position(int* x, int* y);
void reset_mouse_position();


#endif
