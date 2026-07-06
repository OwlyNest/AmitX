/*
	* include/drivers/fb.h - [Enter description]
	* Author:   amity
	* Date:     Tue Jun 23 13:13:21 2026
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
#ifndef __GFX_FB_H__
#define __GFX_FB_H__

/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
typedef struct gfx_surface {
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
	uint32_t pitch;
    uint32_t pitch_px;
} gfx_surface_t;

typedef struct fb_surface {
    gfx_surface_t back;
    uint32_t *front;
    int initialized;
} fb_surface_t;

typedef enum {
	GFX_BG_DESKTOP,
    GFX_BG_PANEL,
    GFX_BG_TITLE,
    GFX_BG_HIGHLIGHT,
    GFX_BG_BUTTON,
    GFX_BG_BUTTON_HOVER,
    GFX_FG_TEXT,
    GFX_FG_TEXT_DIM,
    GFX_FG_ACCENT,
    GFX_BORDER_LIGHT,
    GFX_BORDER_DARK,
    GFX_RED,
    GFX_GREEN,
    GFX_BLUE,
    GFX_YELLOW,
    GFX_WHITE,
    GFX_BLACK,
} gfx_theme_color_t;

/* --- Globals ---*/
extern fb_surface_t fb;
/* --- Prototypes ---*/
int  fb_init(void);
void fb_present(void);                    /* Copy backbuffer → screen + update */

uint32_t fb_pack_pixel(uint8_t r, uint8_t g, uint8_t b);
uint32_t gfx_theme_color(gfx_theme_color_t c);

/* Generic drawing */

void gfx_fill_circle(gfx_surface_t *surface, int cx, int cy, int radius, uint32_t color);
void gfx_draw_line_thick(gfx_surface_t *surface, int x0, int y0, int x1, int y1, int thickness, uint32_t color);
void gfx_draw_vector(gfx_surface_t *surface, int x0, int y0, int angle, int magnitude, int thickness, uint32_t color);
void gfx_draw_arc(gfx_surface_t *surface, int cx, int cy, int radius, int start_angle, int end_angle, uint32_t color);
void gfx_fill_sector(gfx_surface_t *surface, int cx, int cy, int radius, int start_angle, int end_angle, uint32_t color);
void gfx_fill_triangle(gfx_surface_t *surface, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);
void gfx_draw_line_aa(gfx_surface_t *surface, int x0, int y0, int x1, int y1, uint32_t color);
void gfx_clear(gfx_surface_t *surface, uint32_t color);
void gfx_put_pixel(gfx_surface_t *surface, uint32_t x, uint32_t y, uint32_t color);
void gfx_fill_rect(gfx_surface_t *surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_draw_rect(gfx_surface_t *surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_draw_line(gfx_surface_t *surface, int x0, int y0, int x1, int y1, uint32_t color);
void gfx_hline(gfx_surface_t *surface, int x, int y, int w, uint32_t color);
void gfx_vline(gfx_surface_t *surface, int x, int y, int h, uint32_t color);
void gfx_draw_circle(gfx_surface_t *surface, int cx, int cy, int radius, uint32_t color);
void gfx_draw_char(gfx_surface_t *surface, uint32_t x, uint32_t y, char c, uint32_t color);
void gfx_draw_string(gfx_surface_t *surface, uint32_t x, uint32_t y, const char *str, uint32_t color);
int gfx_get_string_width(const char *str);

void gfx_panel(gfx_surface_t *surface, int x, int y, int w, int h, uint32_t bg);
void gfx_bevel_in(gfx_surface_t *surface, int x, int y, int w, int h);
void gfx_bevel_out(gfx_surface_t *surface, int x, int y, int w, int h);
void gfx_title_bar(gfx_surface_t *surface, int x, int y, int w, const char *title);
void gfx_button(gfx_surface_t *surface, int x, int y, int w, int h, const char *label, int pressed);
int ui_button(gfx_surface_t *surface, int x, int y, int w, int h, const char *label);
void gfx_progress_bar(gfx_surface_t *surface, int x, int y, int w, int h, int percent, uint32_t fill, uint32_t empty);
void gfx_list(gfx_surface_t *surface, int x, int y, int w, int h, const char **items, int count, int selected);
void gfx_desktop(gfx_surface_t *surface);
void gfx_status_bar(gfx_surface_t *surface, int x, int y, int w, const char *text);
void gfx_logo_design2(gfx_surface_t *surface, int x, int y);

void fb_fill_circle(int cx, int cy, int radius, uint32_t color);
void fb_draw_line_thick(int x0, int y0, int x1, int y1, int thickness, uint32_t color);
void fb_draw_vector(int x0, int y0, int angle, int magnitude, int thickness, uint32_t color);
void fb_draw_arc(int cx, int cy, int radius, int start_angle, int end_angle, uint32_t color);
void fb_fill_sector(int cx, int cy, int radius, int start_angle, int end_angle, uint32_t color);
void fb_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);
void fb_draw_line_aa(int x0, int y0, int x1, int y1, uint32_t color);
void fb_clear(uint32_t color);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void fb_draw_circle(int cx, int cy, int radius, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color); /* outline */
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t color);
void fb_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color);
int point_in_rect(int px, int py, int x, int y, int w, int h);

#endif