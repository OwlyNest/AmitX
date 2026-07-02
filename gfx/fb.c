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
#include <gfx/font.h>
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
    fb.back.width = svga.width;
    fb.back.height = svga.height;
    fb.back.pitch = svga.pitch;
    fb.back.pitch_px = fb.back.pitch / (svga.bpp / 8);   // Correct for any bpp

    size_t buf_size = (size_t)fb.back.pitch * fb.back.height;
    fb.back.pixels = (uint32_t *)malloc(buf_size);

    if (fb.back.pixels) {
        memset(fb.back.pixels, 0, buf_size);
        printk("[fb] Backbuffer %u KB\n", (uint32_t)(buf_size / 1024));
        fb.initialized = 1;
    } else {
        fb.back.pixels = fb.front;
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

    if (fb.back.pixels != fb.front) {
        memcpy(fb.front, fb.back.pixels, (size_t)fb.back.pitch * fb.back.height);
    }
    svga_update_full();
    mouse_refresh_cursor();
}

/* ==========================================================================
 * Basic Drawing
 * ======================================================================= */
void fb_clear(uint32_t color) {
    if (!fb.initialized) return;
    gfx_clear(&fb.back, color);
}

void gfx_clear(gfx_surface_t *surface, uint32_t color) {
    for (uint32_t y = 0; y < surface->height; y++) {
        uint32_t *row = surface->pixels + y * surface->pitch_px;
        for (uint32_t x = 0; x < surface->width; x++) {
            row[x] = color;
        }
    }
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb.initialized) return;
    gfx_put_pixel(&fb.back, x, y, color);
}
void gfx_put_pixel(gfx_surface_t *surface, uint32_t x, uint32_t y, uint32_t color) {
    if (x >= surface->width || y >= surface->height) return;
    surface->pixels[y * surface->pitch_px + x] = color;
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb.initialized) return;
    gfx_fill_rect(&fb.back, x, y, w, h, color);
}

void gfx_fill_rect(gfx_surface_t *surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (x + w > surface->width)  w = surface->width - x;
    if (y + h > surface->height) h = surface->height - y;
    if (x >= surface->width || y >= surface->height || w == 0 || h == 0) return;

    for (uint32_t row = y; row < y + h; row++) {
        uint32_t *dest = surface->pixels + row * surface->pitch_px + x;
        for (uint32_t col = 0; col < w; col++) {
            dest[col] = color;
        }
    }
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb.initialized) return;
    gfx_draw_rect(&fb.back, x, y, w, h, color);
}
void gfx_draw_rect(gfx_surface_t *surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    gfx_draw_line(surface, x, y, x+w, y, color);
    gfx_draw_line(surface, x, y, x, y+h, color);
    gfx_draw_line(surface, x+w, y, x+w, y+h, color);
    gfx_draw_line(surface, x, y+h, x+w, y+h, color);
}

/* ==========================================================================
 * Bresenham's Line Algorithm
 * ======================================================================= */
int abs(int x){
    return x < 0 ? -x : x;
}

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    if (!fb.initialized) return;
    gfx_draw_line(&fb.back, x0, y0, x1, y1, color);
}

void gfx_draw_line(gfx_surface_t *surface, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        gfx_put_pixel(surface, x0, y0, color);
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
    if (!fb.initialized) return;
    gfx_draw_circle(&fb.back, cx, cy, radius, color);
}

void gfx_draw_circle(gfx_surface_t *surface, int cx, int cy, int radius, uint32_t color) {
    if (radius <= 0) return;

    int x = radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);

    while (x >= y) {
        gfx_put_pixel(surface, cx + x, cy + y, color);
        gfx_put_pixel(surface, cx + y, cy + x, color);
        gfx_put_pixel(surface, cx - y, cy + x, color);
        gfx_put_pixel(surface, cx - x, cy + y, color);
        gfx_put_pixel(surface, cx - x, cy - y, color);
        gfx_put_pixel(surface, cx - y, cy - x, color);
        gfx_put_pixel(surface, cx + y, cy - x, color);
        gfx_put_pixel(surface, cx + x, cy - y, color);

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

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t color) {
    if (!fb.initialized) return;
    gfx_draw_char(&fb.back, x, y, c, color);
}
void gfx_draw_char(gfx_surface_t *surface, uint32_t x, uint32_t y, char c, uint32_t color) {
    if (x >= surface->width || y >= surface->height) return;
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc >= 128) uc = '?';

    const uint8_t *glyph = font8x8[uc];

    for (int row = 0; row < 8; row++) {
        if (y + row >= surface->height) break;
        uint8_t line = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (x + col >= fb.back.width) break;
            if (line & (1u << (7 - col))) {
                gfx_put_pixel(surface, x + col, y + row, color);
            }
        }
    }
}

void fb_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color) {
    if (!fb.initialized) return;
    gfx_draw_string(&fb.back, x, y, str, color);
}
void gfx_draw_string(gfx_surface_t *surface, uint32_t x, uint32_t y, const char *str, uint32_t color) {
    if (!str) return;

    uint32_t cx = x;
    while (*str) {
        gfx_draw_char(surface, cx, y, *str++, color);
        cx += 8;
    }
}