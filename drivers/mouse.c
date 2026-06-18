#include <drivers/mouse.h>
#include <arch/x86/io.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <screen/screen.h>
#include <arch/x86/interrupts.h>
#include <internal/amitx_consts.h>
#include <screen/printk.h>
/*
    * Get ready for the buggyest mouse you'll ever use
    * if you run in a VM, try to ignore your native mouse
    * I'm really trying my best here
    * 
    * It finally works
    * yay!
*/

static int mouse_cycle = 0;
static int8_t mouse_bytes[3];
static int mouse_px_x, mouse_px_y;
int mouse_x = 40, mouse_y = 12;
uint8_t mouse_buttons = 0;

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if (inb(PORT_KBD_STATUS) & 1) return;
        }
    } else {
        while (timeout--) {
            if (!(inb(PORT_KBD_STATUS) & 2)) return;
        }
    }
}

static void mouse_write(uint8_t value) {
    mouse_wait(1);
    outb(PORT_KBD_CMD, MOUSE_CMD_SEND_TO_DEV);
    mouse_wait(1);
    outb(PORT_KBD_DATA, value);
}

static uint8_t mouse_read() {
    mouse_wait(0);
    return inb(PORT_KBD_DATA);
}

void mouse_handler(interrupt_frame_t *frame) {
    (void)frame;
    uint8_t status = inb(PORT_KBD_STATUS);
    if (!(status & 1)) return;

    int8_t data = inb(PORT_KBD_DATA);

    switch (mouse_cycle) {
        case 0:
            if (!(data & 0x08)) {
                break;
            }
            mouse_bytes[0] = data;
            mouse_cycle++;
            break;
        case 1:
            mouse_bytes[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_bytes[2] = data;

            int dx = mouse_bytes[1];
            int dy = -mouse_bytes[2];

            mouse_px_x += dx;
            mouse_px_y += dy;

            if (mouse_px_x < 0) mouse_px_x = 0;
            if (mouse_px_y < 0) mouse_px_y = 0;
            if (mouse_px_x >= 639) mouse_px_x = 639;
            if (mouse_px_y >= 399) mouse_px_y = 399;

            mouse_x = mouse_px_x / 8;
            mouse_y = mouse_px_y / 16;

            draw_mouse_cursor();

            mouse_buttons = mouse_bytes[0] & 0x07;

            mouse_cycle = 0;
            break;
    }
}

static int init_mouse() {
    uint8_t status;

    mouse_wait(1);
    outb(PORT_KBD_CMD, MOUSE_CMD_ENABLE_AUX);

    mouse_wait(1);
    outb(PORT_KBD_CMD, MOUSE_CMD_READ_CFG);
    mouse_wait(0);
    status = (inb(PORT_KBD_DATA) | 2);
    mouse_wait(1);
    outb(PORT_KBD_CMD, MOUSE_CMD_WRITE_CFG);
    mouse_wait(1);
    outb(PORT_KBD_DATA, status);

    mouse_write(MOUSE_CMD_SET_DEFAULTS); mouse_read();
    mouse_write(MOUSE_CMD_ENABLE_STREAM); mouse_read();

    register_interrupt_handler(VECTOR_IRQ12, mouse_handler);
    printk("Mouse initialized.\n");
    pic_unmask_irq(12);
    pic_unmask_irq(2);
    return 0;
}

kscope_node_t mouse_node = {
    .name = "PS/2-mouse",
    .id = 0x000C,
    .class = KSCOPE_CLASS_DRIVER,
    .subclass = KSCOPE_SUBCLASS_DRIVER_MOUSE,
    .requires = (kscope_node_t*[]){&x86_pic_node, &x86_idt_node},
    .require_count = 2,
    .provides = (const char*[]){"input.mouse", "irq.12"},
    .provide_count = 2,
    .init = init_mouse,
};

void get_mouse_position(int* x, int* y) {
    *x = mouse_x;
    *y = mouse_y;
}

void reset_mouse_position() {
    mouse_px_x = 320;
    mouse_px_y = 200;
    mouse_x = 40;
    mouse_y = 12;
    mouse_cycle = 0;
}