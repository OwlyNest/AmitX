#include "timer.h"
#include "amitx_consts.h"
#include "io.h"
#include "screen.h"
#include "amitx_consts.h"
#include <stdint.h>

extern void register_interrupt_handler(int n, void (*handler)());

static void (*timer_handler)() = 0;

void timer_callback_wrapper() {
    if (timer_handler) timer_handler();
}

void init_timer(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_HZ / frequency;

    // Send command byte
    outb(PORT_PIT_CMD, PIT_MODE_SQUARE_WAVE); // binary, mode 3 (square wave), lobyte/hibyte, channel 0

    // Send frequency divisor
    outb(PORT_PIT_CH0, divisor & 0xFF);        // Low byte
    outb(PORT_PIT_CH0, (divisor >> 8) & 0xFF); // High byte

    // Register ISR 32 (first IRQ remapped) for our timer
    timer_handler = timer_callback;
    register_interrupt_handler(VECTOR_IRQ0, timer_callback_wrapper);
    
    puts("[init_timer] Timer initialized\n");

    __asm__ __volatile__ ("sti");
}