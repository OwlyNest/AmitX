/*
    * drivers/mouse.c - PS/2 mouse driver
    * Author:   amity
    * Date:     Sat Jun 20 22:58:35 2026
    * Copyright © 2026 OwlyNest
*/

/* --- Styling Instructions ---
    * Encoding:                      UTF-8, Unix line endings
    * Text font:                     Monospace
    * Line width:                    Max 80 characters
    * Indentation:                   Use 4 spaces
    * Brace style:                   Same line as control statement
    * Inline comments:               Column 40, wherever possible, else, whole multiple of 20
    * Section headers:               Use 3 '-' characters before and after
    * Pointer notation:              Next to variable name, not type
    * Binary operations:             Space around operator
    * Empty parameter list:          Use (void) instead of ()
    * Statements and declarations:   Max one per line
*/

/* --- Macros ---*/
#define CURSOR_W 11
#define CURSOR_H 11
/* --- Includes ---*/
#include <drivers/mouse.h>
#include <gfx/fb.h>
#include <hw/svga.h>
#include <arch/x86/io.h>
#include <arch/x86/interrupts.h>
#include <internal/amitx_consts.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static uint32_t cursor_under[CURSOR_W * CURSOR_H];
static int cursor_prev_x = -1;
static int cursor_prev_y = -1;

static const uint8_t cursor_arrow[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0},
    {1,1,1,1,1,1,1,1,1,1,0},
    {0,0,0,0,0,0,0,0,0,0,0},
};

static const uint8_t cursor_left[CURSOR_H][CURSOR_W] = {
    {1,1,1,1,1,1,1,1,1,1,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
};

static const uint8_t cursor_right[CURSOR_H][CURSOR_W] = {
    {1,1,1,1,1,0,0,0,0,0,0},
    {1,1,1,1,1,0,0,0,0,0,0},
    {1,1,1,1,1,0,0,0,0,0,0},
    {1,1,1,1,1,0,0,0,0,0,0},
    {1,1,1,1,1,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
};

/* Raw pixel coordinates */
static volatile int mouse_px_x = 512;
static volatile int mouse_px_y = 384;

/* Pixel coordinates exposed to UI */
volatile int mouse_x = 512;
volatile int mouse_y = 384;
volatile uint8_t mouse_buttons = 0;
static uint8_t mouse_buttons_prev = 0;

/* Packet reassembly state */
static volatile int mouse_cycle = 0;
static volatile int8_t mouse_bytes[3];

/* --- Prototypes ---*/
static void mouse_wait_output(void);
static void mouse_wait_input(void);
static uint8_t mouse_read_byte(void);
static int mouse_send_cmd(uint8_t cmd);
static int mouse_handler(interrupt_frame_t *frame);

/* --- Functions ---*/
/* --- Cursor helpers --- */
static void cursor_erase_front(void) {
    if (cursor_prev_x < 0 || !fb.initialized || !fb.front) return;
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            int px = cursor_prev_x + col;
            int py = cursor_prev_y + row;
            if (px >= 0 && px < (int)fb.back.width &&
                py >= 0 && py < (int)fb.back.height) {
                fb.front[py * fb.back.pitch_px + px] =
                    cursor_under[row * CURSOR_W + col];
            }
        }
    }
    if (svga.initialized) {
        svga_update_rect(cursor_prev_x, cursor_prev_y,
                         CURSOR_W, CURSOR_H);
    }
}

static void cursor_draw_front(int x, int y) {
    if (!fb.initialized || !fb.front) return;
    
    /* Save what's under the new position */
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < (int)fb.back.width &&
                py >= 0 && py < (int)fb.back.height) {
                cursor_under[row * CURSOR_W + col] =
                    fb.front[py * fb.back.pitch_px + px];
            }
        }
    }
    
    /* Draw cursor */
    const uint8_t (*cursor)[CURSOR_W] = cursor_arrow;

    if (mouse_buttons & 1)
        cursor = cursor_left;
    else if (mouse_buttons & 2)
        cursor = cursor_right;

    uint32_t white = gfx_theme_color(GFX_WHITE);
    uint32_t black = gfx_theme_color(GFX_BLACK);
    uint32_t color = white;

    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            if (!cursor[row][col])
                continue;

            if (cursor[row][col] == 1) {
                color = white;
            } else if (cursor[row][col] == 2) {
                color = black;
            }

            int px = x + col;
            int py = y + row;

            if (px >= 0 && px < (int)fb.back.width && py >= 0 && py < (int)fb.back.height) {
                fb.front[py * fb.back.pitch_px + px] = color;
            }
        }
    }
    
    cursor_prev_x = x;
    cursor_prev_y = y;
    if (svga.initialized) {
        svga_update_rect(x, y, CURSOR_W, CURSOR_H);
    }
}


/* ==========================================================================
 * Wait for PS/2 controller output buffer to have data (bit 0 set)
 * ======================================================================= */
static void mouse_wait_output(void) {
    for (uint32_t timeout = 100000; timeout; timeout--) {
        if (inb(PORT_KBD_STATUS) & 0x01)
            return;
    }
}

/* ==========================================================================
 * Wait for PS/2 controller input buffer to be empty (bit 1 clear)
 * ======================================================================= */
static void mouse_wait_input(void) {
    for (uint32_t timeout = 100000; timeout; timeout--) {
        if (!(inb(PORT_KBD_STATUS) & 0x02))
            return;
    }
}

/* ==========================================================================
 * Read one byte from PS/2 data port
 * ======================================================================= */
static uint8_t mouse_read_byte(void) {
    mouse_wait_output();
    return inb(PORT_KBD_DATA);
}

/* ==========================================================================
 * Send a command to the mouse device via the PS/2 controller.
 * Returns 0 if ACK (0xFA) received, -1 on failure.
 * ======================================================================= */
static int mouse_send_cmd(uint8_t cmd) {
    for (int retry = 0; retry < 3; retry++) {
        mouse_wait_input();
        outb(PORT_KBD_CMD, MOUSE_CMD_SEND_TO_DEV);
        mouse_wait_input();
        outb(PORT_KBD_DATA, cmd);

        uint8_t resp = mouse_read_byte();
        if (resp == 0xFA)  /* ACK */
            return 0;
        if (resp == 0xFE)  /* RESEND */
            continue;

        printk("[mouse] Unexpected response 0x%02x to cmd 0x%02x\n",
               resp, cmd);
        return -1;
    }
    printk("[mouse] Command 0x%02x failed after 3 retries\n", cmd);
    return -1;
}

/* ==========================================================================
 * IRQ12 handler
 *
 * ONLY updates coordinates.  NEVER draws here — the UI loop handles
 * all rendering and presentation.
 * ======================================================================= */
static int mouse_handler(interrupt_frame_t *frame) {
    (void)frame;

    uint8_t status = inb(PORT_KBD_STATUS);

    /* Bit 0: output buffer full. Bit 5: auxiliary device data. */
    if (!(status & 0x01) || !(status & 0x20))
        return 0;

    int8_t data = (int8_t)inb(PORT_KBD_DATA);

    switch (mouse_cycle) {
    case 0:
        /* First byte: bit 3 must be set (sync check) */
        if (!(data & 0x08))
            break;
        mouse_bytes[0] = data;
        mouse_cycle = 1;
        break;

    case 1:
        mouse_bytes[1] = data;
        mouse_cycle = 2;
        break;

    case 2:
        mouse_bytes[2] = data;

        /* Decode 9-bit signed movement values */
        int dx = (int)(uint8_t)mouse_bytes[1];
        int dy = (int)(uint8_t)mouse_bytes[2];

        if (mouse_bytes[0] & 0x10)
            dx -= 256;
        if (mouse_bytes[0] & 0x20)
            dy -= 256;

        mouse_px_x += dx;
        mouse_px_y -= dy;  /* Y is inverted on screen */

        /* Clamp to screen bounds */
        int max_x = fb.initialized ? (int)fb.back.width  - 1 : 1023;
        int max_y = fb.initialized ? (int)fb.back.height - 1 : 767;

        if (mouse_px_x < 0) mouse_px_x = 0;
        if (mouse_px_y < 0) mouse_px_y = 0;
        if (mouse_px_x > max_x) mouse_px_x = max_x;
        if (mouse_px_y > max_y) mouse_px_y = max_y;

        mouse_x = mouse_px_x;
        mouse_y = mouse_px_y;

        /* Button state: lower 3 bits of first byte */
        mouse_buttons_prev = mouse_buttons;
        mouse_buttons = mouse_bytes[0] & 0x07;

        cursor_erase_front();
        cursor_draw_front(mouse_x, mouse_y);

        mouse_cycle = 0;

        break;
    }
    return 1;
}

/* ==========================================================================
 * KScope init
 * ======================================================================= */
static int init_mouse(void) {
    /* Enable auxiliary device port */
    mouse_wait_input();
    outb(PORT_KBD_CMD, MOUSE_CMD_ENABLE_AUX);

    /* Read and modify controller configuration */
    mouse_wait_input();
    outb(PORT_KBD_CMD, MOUSE_CMD_READ_CFG);
    uint8_t cfg = mouse_read_byte();

    cfg |= 0x02;   /* Enable IRQ12 */
    cfg &= ~0x20;  /* Ensure mouse clock is not disabled */

    mouse_wait_input();
    outb(PORT_KBD_CMD, MOUSE_CMD_WRITE_CFG);
    mouse_wait_input();
    outb(PORT_KBD_DATA, cfg);

    /* Reset mouse and verify self-test */
    if (mouse_send_cmd(0xFF) != 0) {
        printk("[mouse] No mouse detected (reset failed)\n");
        return -1;
    }

    uint8_t self_test = mouse_read_byte();
    uint8_t mouse_id  = mouse_read_byte();

    if (self_test != 0xAA) {
        printk("[mouse] Self-test failed (0x%02x), disabling\n", self_test);
        return -1;
    }

    /* Set defaults and enable streaming */
    if (mouse_send_cmd(MOUSE_CMD_SET_DEFAULTS) != 0) {
        printk("[mouse] Set defaults failed\n");
        return -1;
    }

    if (mouse_send_cmd(MOUSE_CMD_ENABLE_STREAM) != 0) {
        printk("[mouse] Enable stream failed\n");
        return -1;
    }

    register_interrupt_handler(VECTOR_IRQ12, mouse_handler);
    pic_unmask_irq(12);
    pic_unmask_irq(2);  /* Slave PIC cascade */

    printk("[mouse] Initialized (ID 0x%02x)\n", mouse_id);
    return 0;
}

kscope_node_t mouse_node = {
    .name = "PS/2-mouse",
    .id = 0x000C,
    .class = KSCOPE_CLASS_DRIVER,
    .subclass = KSCOPE_SUBCLASS_DRIVER_MOUSE,
    .requires = (kscope_node_t *[]){&x86_pic_node, &x86_idt_node},
    .require_count = 2,
    .provides = (const char *[]){"input.mouse", "irq.12"},
    .provide_count = 2,
    .init = init_mouse,
};

/* ==========================================================================
 * Get current mouse position in pixel coordinates
 * ======================================================================= */
void get_mouse_position(int *x, int *y) {
    *x = mouse_x;
    *y = mouse_y;
}

void mouse_refresh_cursor(void) {
    if (!fb.initialized || !fb.front) return;

    cursor_prev_x = -1;
    cursor_draw_front(mouse_x, mouse_y);
}

/* ==========================================================================
 * Reset mouse position to center of screen
 * ======================================================================= */
void reset_mouse_position(void) {
    int cx = fb.initialized ? (int)fb.back.width  / 2 : 512;
    int cy = fb.initialized ? (int)fb.back.height / 2 : 384;

    mouse_px_x = cx;
    mouse_px_y = cy;
    mouse_x = cx;
    mouse_y = cy;
    mouse_cycle = 0;
    mouse_bytes[0] = 0;
    mouse_bytes[1] = 0;
    mouse_bytes[2] = 0;
}

int mouse_button_state(void) {
    return mouse_buttons;
}

int mouse_left_down(void) {
    return mouse_buttons & 1;
}

int mouse_left_pressed(void) {
    return !(mouse_buttons_prev & 1) && (mouse_buttons & 1);
}

int mouse_left_released(void) {
    return (mouse_buttons_prev & 1) && !(mouse_buttons & 1);
}

int mouse_right_down(void) {
    return mouse_buttons & 2;
}