
#ifndef TIME_H
#define TIME_H

#include <stdint.h>

void timer_callback();
void sleep(uint32_t seconds);
void sleep_ms(uint32_t miliseconds);
void sleep_t(uint32_t ticks);

#endif
