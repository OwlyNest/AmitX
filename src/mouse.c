#include "mouse.h"
#include "io.h"
#include "screen.h"
#include "interrupts.h"

#define MOUSE_DATA 0x60
#define MOUSE_STATUS 0x64
#define MOUSE_CMD 0x64

static int mouse_cycle = 0;
static int8_t mouse_bytes[3];
static int mouse_px_x, mouse_px_y;
int mouse_x = 40, mouse_y = 12; // Start near center of 80x25
uint8_t mouse_buttons = 0;

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if (inb(MOUSE_STATUS) & 1) return;
        }
    } else {
        while (timeout--) {
            if (!(inb(MOUSE_STATUS) & 2)) return;
        }
    }
}

static void mouse_write(uint8_t value) {
    mouse_wait(1);
    outb(MOUSE_CMD, 0xD4);
    mouse_wait(1);
    outb(MOUSE_DATA, value);
}

static uint8_t mouse_read() {
    mouse_wait(0);
    return inb(MOUSE_DATA);
}

void mouse_handler() {
    uint8_t status = inb(MOUSE_STATUS);
    if (!(status & 1)) return;

    int8_t data = inb(MOUSE_DATA);

    switch (mouse_cycle) {
        case 0:
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

            mouse_buttons = mouse_bytes[0] & 0x07; // L, R, Middle

            mouse_cycle = 0;
            break;
    }
}

void init_mouse() {
    uint8_t status;

    // Enable the auxiliary mouse device
    mouse_wait(1);
    outb(MOUSE_CMD, 0xA8);

    // Enable interrupts
    mouse_wait(1);
    outb(MOUSE_CMD, 0x20);
    mouse_wait(0);
    status = (inb(MOUSE_DATA) | 2);
    mouse_wait(1);
    outb(MOUSE_CMD, 0x60);
    mouse_wait(1);
    outb(MOUSE_DATA, status);

    // Tell mouse to use default settings
    mouse_write(0xF6); mouse_read();

    // Enable mouse
    mouse_write(0xF4); mouse_read();

    // Register handler
    register_interrupt_handler(44, mouse_handler);
    puts("Mouse initialized.\n");
}

void get_mouse_position(int* x, int* y) {
    *x = mouse_x;
    *y = mouse_y;
}