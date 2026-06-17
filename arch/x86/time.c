#include <arch/x86/time.h>
#include <arch/x86/timer.h>
#include <screen/screen.h>
#include <stdint.h>

volatile uint32_t tick_count = 0;
extern int menu;

/*
    * PIT is 100 Hz
*/

void timer_callback() {
    tick_count++;
    if (menu) {
        draw_uptime();
    }

}
void sleep(uint32_t seconds) {
    uint32_t start = tick_count;
    uint32_t target = seconds * 100;
    while ((tick_count - start) < target) {
        __asm__ __volatile__("hlt");
    }
}
void sleep_ms(uint32_t miliseconds) {
    uint32_t start = tick_count;
    uint32_t target = miliseconds / 10;
    while ((tick_count - start) < (target)) {
        __asm__ __volatile__("hlt");
    }
}

void sleep_t(uint32_t ticks) {
    __asm__ __volatile__ ("sti");
    uint32_t start = tick_count;
    while ((tick_count - start) < (ticks)) {
    }
}