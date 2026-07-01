/*
	* gfx/fb.c - [Enter description]
	* Author:   amity
	* Date:     Tue Jun 23 13:13:14 2026
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

/* --- Includes ---*/
#include <gfx/fb.h>
#include <hw/svga.h>
#include <drivers/mouse.h>
#include <screen/printk.h>
#include <mm/heap.h>
#include <lib/string.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
fb_surface_t fb = { 0 };
/* --- Prototypes ---*/

/* --- Functions ---*/
/* ==========================================================================
 * Initialize Framebuffer
 * ======================================================================= */
 int fb_init(void) {
    if (!svga.initialized || !svga.fb_virt) {
        printk("[fb] SVGA not ready\n");
        return -1;
    }

    fb.front = (uint32_t *)svga.fb_virt;
    fb.width = svga.width;
    fb.height = svga.height;
    fb.pitch = svga.pitch;
    fb.pitch_px = fb.pitch / (svga.bpp / 8);   // Correct for any bpp

    size_t buf_size = (size_t)fb.pitch * fb.height;
    fb.pixels = (uint32_t *)malloc(buf_size);

    if (fb.pixels) {
        memset(fb.pixels, 0, buf_size);
        printk("[fb] Backbuffer %u KB\n", (uint32_t)(buf_size / 1024));
        fb.initialized = 1;
    } else {
        fb.pixels = fb.front;
        printk("[fb] Direct FB mode\n");
        fb.initialized = 1;
    }
    return 0;
}

/* ==========================================================================
 * Present backbuffer to screen
 * ======================================================================= */
void fb_present(void) {
    if (!fb.initialized) return;

    if (fb.pixels != fb.front) {
        memcpy(fb.front, fb.pixels, (size_t)fb.pitch * fb.height);
    }
    svga_update_full();
    mouse_refresh_cursor();
}

/* ==========================================================================
 * Basic Drawing
 * ======================================================================= */
void fb_clear(uint32_t color) {
    if (!fb.initialized) return;
    for (uint32_t y = 0; y < fb.height; y++) {
        uint32_t *row = fb.pixels + y * fb.pitch_px;
        for (uint32_t x = 0; x < fb.width; x++) {
            row[x] = color;
        }
    }
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb.initialized || x >= fb.width || y >= fb.height) return;
    fb.pixels[y * fb.pitch_px + x] = color;
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb.initialized) return;
    if (x + w > fb.width)  w = fb.width - x;
    if (y + h > fb.height) h = fb.height - y;
    if (x >= fb.width || y >= fb.height || w == 0 || h == 0) return;

    for (uint32_t row = y; row < y + h; row++) {
        uint32_t *dest = fb.pixels + row * fb.pitch_px + x;
        for (uint32_t col = 0; col < w; col++) {
            dest[col] = color;
        }
    }
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb.initialized) return;
    fb_draw_line(x, y, x+w, y, color);
    fb_draw_line(x, y, x, y+h, color);
    fb_draw_line(x+w, y, x+w, y+h, color);
    fb_draw_line(x, y+h, x+w, y+h, color);
}

/* ==========================================================================
 * Bresenham's Line Algorithm
 * ======================================================================= */
int abs(int x){
    return x < 0 ? -x : x;
}

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    if (!fb.initialized) return;

    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        fb_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ==========================================================================
 * Circle (midpoint algorithm)
 * ======================================================================= */
void fb_draw_circle(int cx, int cy, int radius, uint32_t color) {
    if (!fb.initialized || radius <= 0) return;

    int x = radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);

    while (x >= y) {
        fb_put_pixel(cx + x, cy + y, color);
        fb_put_pixel(cx + y, cy + x, color);
        fb_put_pixel(cx - y, cy + x, color);
        fb_put_pixel(cx - x, cy + y, color);
        fb_put_pixel(cx - x, cy - y, color);
        fb_put_pixel(cx - y, cy - x, color);
        fb_put_pixel(cx + y, cy - x, color);
        fb_put_pixel(cx + x, cy - y, color);

        if (err <= 0) {
            y++;
            err += dy;
            dy += 2;
        }
        if (err > 0) {
            x--;
            dx += 2;
            err += dx - (radius << 1);
        }
    }
}