#include <arch/x86/timer.h>
#include <arch/x86/io.h>
#include <screen/screen.h>
#include <arch/x86/interrupts.h>
#include <screen/printk.h>
#include <internal/amitx_consts.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <stdint.h>

extern void register_interrupt_handler(int n, void (*handler)());

static void (*timer_handler)() = 0;

void timer_callback_wrapper(interrupt_frame_t *frame) {
    (void)frame;
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
    
    printk("[timer] Timer initialized\n");
    __asm__ __volatile__ ("sti");
    pic_unmask_irq(0);
}

static int timer_kscope_init(void) {
    init_timer(100);
    return 0;
}

kscope_node_t pit_timer_node = {
    .name = "pit-timer",
    .id = 0x0004,
    .class = KSCOPE_CLASS_TIME,
    .subclass = KSCOPE_SUBCLASS_TIME_PIT,
    .requires = (kscope_node_t *[]){ &x86_pic_node, &x86_idt_node },
    .require_count = 1,
    .provides = (const char *[]){"time.pit", "irq.0", "sched.timer"},
    .provide_count = 3,
    .init = timer_kscope_init
};