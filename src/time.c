#include "time.h"
#include "timer.h"
#include "kernel.h"
#include "screen.h"
#include <stdint.h>

volatile uint32_t tick_count = 0;
extern int menu;

void timer_callback() {
    tick_count++;
    if (menu) {
        draw_uptime();
    }

}
void sleep(uint32_t seconds) {
    __asm__ __volatile__ ("sti");
    uint32_t start = tick_count;
    while ((tick_count - start) < (seconds * 100)) {
        // 18.2 Hz by default if PIT isn't configured.
        // Replace 18 with real frequency if configured.
    }
}
void sleep_ms(uint32_t miliseconds) {
    __asm__ __volatile__ ("sti");
    uint32_t start = tick_count;
    while ((tick_count - start) < (miliseconds / 10)) {
        // 18.2 Hz by default if PIT isn't configured.
        // Replace 18 with real frequency if configured.
    }
}

void sleep_t(uint32_t ticks) {
    __asm__ __volatile__ ("sti");
    uint32_t start = tick_count;
    while ((tick_count - start) < (ticks)) {
        // 18.2 Hz by default if PIT isn't configured.
        // Replace 18 with real frequency if configured.
    }
}