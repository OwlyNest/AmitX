#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

void init_mouse();
void mouse_handler();
void get_mouse_position(int* x, int* y);
void reset_mouse_position();


#endif
