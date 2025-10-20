#include "timer.h"
#include "screen.h"
#include "io.h"
#include <stdint.h>

extern void register_interrupt_handler(int n, void (*handler)());

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQUENCY 1193182

static void (*timer_handler)() = 0;

void timer_callback_wrapper() {
    if (timer_handler) timer_handler();
}

void init_timer(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;

    // Send command byte
    outb(PIT_COMMAND, 0x36); // binary, mode 3 (square wave), lobyte/hibyte, channel 0

    // Send frequency divisor
    outb(PIT_CHANNEL0, divisor & 0xFF);        // Low byte
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF); // High byte

    // Register ISR 32 (first IRQ remapped) for our timer
    timer_handler = timer_callback;
    register_interrupt_handler(32, timer_callback_wrapper);
    
    puts("[init_timer] Timer initialized\n");

    __asm__ __volatile__ ("sti");
}