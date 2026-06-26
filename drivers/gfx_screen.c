/*
	* drivers/gfx_screen.c - [Enter description]
	* Author:   amity
	* Date:     Wed Jun 24 16:55:49 2026
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
#include <drivers/gfx_screen.h>
#include <drivers/fb.h>
#include <drivers/font.h>
#include <screen/printk.h>
#include <lib/string.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
gfx_screen_t gscreen = { 0 };
extern int tick_count;

/* --- Cursor backing store ---*/
static int cursor_sx = -1;
static int cursor_sy = -1;

/* --- Prototypes ---*/
static int clip_rect(int *x, int *y, int *w, int *h);

/* --- Functions ---*/

/* ==========================================================================
 * Color helpers
 * ======================================================================= */
uint32_t gfx_theme_color(gfx_theme_color_t c) {
    switch (c) {
        case GFX_BG_DESKTOP:    return FB_RGB(20, 30, 100);
        case GFX_BG_PANEL:      return FB_RGB(40, 45, 60);
        case GFX_BG_TITLE:      return FB_RGB(80, 90, 120);
        case GFX_BG_HIGHLIGHT:  return FB_RGB(100, 120, 160);
        case GFX_BG_BUTTON:     return FB_RGB(60, 70, 90);
        case GFX_BG_BUTTON_HOVER: return FB_RGB(80, 95, 120);
        case GFX_FG_TEXT:       return FB_RGB(255, 255, 255);
        case GFX_FG_TEXT_DIM:   return FB_RGB(180, 180, 200);
        case GFX_FG_ACCENT:     return FB_RGB(100, 200, 255);
        case GFX_BORDER_LIGHT:  return FB_RGB(120, 130, 150);
        case GFX_BORDER_DARK:   return FB_RGB(20, 25, 35);
        case GFX_RED:           return FB_RGB(255, 0, 0);
        case GFX_GREEN:         return FB_RGB(0, 255, 0);
        case GFX_BLUE:          return FB_RGB(0, 0, 255);
        case GFX_YELLOW:        return FB_RGB(255, 255, 0);
        case GFX_WHITE:         return FB_RGB(255, 255, 255);
        case GFX_BLACK:         return FB_RGB(0, 0, 0);
        default:                return FB_RGB(255, 255, 255);
    }
}

/* ==========================================================================
 * Clipping
 * ======================================================================= */
static int clip_rect(int *x, int *y, int *w, int *h) {
    if (!gscreen.clip_enabled) return 1;

    int cx = gscreen.clip_x;
    int cy = gscreen.clip_y;
    int cw = gscreen.clip_w;
    int ch = gscreen.clip_h;

    if (*x >= cx + cw || *y >= cy + ch ||
        *x + *w <= cx || *y + *h <= cy) {
        return 0;
    }

    if (*x < cx) {
        int diff = cx - *x;
        *x += diff;
        *w -= diff;
    }
    if (*y < cy) {
        int diff = cy - *y;
        *y += diff;
        *h -= diff;
    }
    if (*x + *w > cx + cw) {
        *w = cx + cw - *x;
    }
    if (*y + *h > cy + ch) {
        *h = cy + ch - *y;
    }
    return 1;
}

void gfx_set_clip(int x, int y, int w, int h) {
    gscreen.clip_enabled = 1;
    gscreen.clip_x = x;
    gscreen.clip_y = y;
    gscreen.clip_w = w;
    gscreen.clip_h = h;
}

void gfx_clear_clip(void) {
    gscreen.clip_enabled = 0;
}

/* ==========================================================================
 * Primitive wrappers with clipping
 * ======================================================================= */
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (clip_rect(&x, &y, &w, &h)) {
        fb_fill_rect((uint32_t)x, (uint32_t)y,
                     (uint32_t)w, (uint32_t)h, color);
    }
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    int x2 = x + w - 1;
    int y2 = y + h - 1;
    fb_draw_line(x, y, x2, y, color);
    fb_draw_line(x2, y, x2, y2, color);
    fb_draw_line(x2, y2, x, y2, color);
    fb_draw_line(x, y2, x, y, color);
}

void gfx_hline(int x, int y, int w, uint32_t color) {
    int h = 1;
    if (clip_rect(&x, &y, &w, &h)) {
        fb_fill_rect((uint32_t)x, (uint32_t)y,
                     (uint32_t)w, 1, color);
    }
}

void gfx_vline(int x, int y, int h, uint32_t color) {
    int w = 1;
    if (clip_rect(&x, &y, &w, &h)) {
        fb_fill_rect((uint32_t)x, (uint32_t)y,
                     1, (uint32_t)h, color);
    }
}

/* ==========================================================================
 * Text
 * ======================================================================= */
void gfx_draw_text(int x, int y, const char *str, uint32_t color) {
    if (!str || !fb.initialized) return;

    int cx = x;
    int cy = y;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += 8;
            str++;
            continue;
        }
        if (gscreen.clip_enabled) {
            if (cx >= gscreen.clip_x + gscreen.clip_w ||
                cy >= gscreen.clip_y + gscreen.clip_h ||
                cx + 8 <= gscreen.clip_x ||
                cy + 8 <= gscreen.clip_y) {
                cx += 8;
                str++;
                continue;
            }
        }
        fb_draw_char((uint32_t)cx, (uint32_t)cy, *str, color);
        cx += 8;
        str++;
    }
}

int gfx_text_width(const char *str) {
    return fb_get_string_width(str);
}

/* ==========================================================================
 * Panels and 3D borders
 * ======================================================================= */
void gfx_panel(int x, int y, int w, int h, uint32_t bg) {
    gfx_fill_rect(x, y, w, h, bg);
}

void gfx_bevel_in(int x, int y, int w, int h) {
    uint32_t light = gfx_theme_color(GFX_BORDER_LIGHT);
    uint32_t dark  = gfx_theme_color(GFX_BORDER_DARK);
    gfx_hline(x, y, w, light);
    gfx_vline(x, y, h, light);
    gfx_hline(x, y + h - 1, w, dark);
    gfx_vline(x + w - 1, y, h, dark);
}

void gfx_bevel_out(int x, int y, int w, int h) {
    uint32_t light = gfx_theme_color(GFX_BORDER_LIGHT);
    uint32_t dark  = gfx_theme_color(GFX_BORDER_DARK);
    gfx_hline(x, y, w, dark);
    gfx_vline(x, y, h, dark);
    gfx_hline(x, y + h - 1, w, light);
    gfx_vline(x + w - 1, y, h, light);
}

/* ==========================================================================
 * Title bar
 * ======================================================================= */
void gfx_title_bar(int x, int y, int w, const char *title) {
    uint32_t bg = gfx_theme_color(GFX_BG_TITLE);
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);

    gfx_fill_rect(x, y, w, 20, bg);
    if (title) {
        gfx_draw_text(x + 4, y + 6, title, fg);
    }
    gfx_bevel_out(x, y, w, 20);
}

/* ==========================================================================
 * Button
 * ======================================================================= */
void gfx_button(int x, int y, int w, int h,
                const char *label, int pressed) {
    uint32_t bg = pressed
                  ? gfx_theme_color(GFX_BG_BUTTON_HOVER)
                  : gfx_theme_color(GFX_BG_BUTTON);
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);

    gfx_fill_rect(x, y, w, h, bg);
    if (pressed) {
        gfx_bevel_in(x, y, w, h);
    } else {
        gfx_bevel_out(x, y, w, h);
    }

    if (label) {
        int tw = gfx_text_width(label);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - 8) / 2;
        gfx_draw_text(tx, ty, label, fg);
    }
}

/* ==========================================================================
 * Progress bar
 * ======================================================================= */
void gfx_progress_bar(int x, int y, int w, int h,
                      int percent, uint32_t fill, uint32_t empty) {
    gfx_fill_rect(x, y, w, h, empty);
    gfx_bevel_in(x, y, w, h);

    int fill_w = (w - 4) * percent / 100;
    if (fill_w > 0) {
        gfx_fill_rect(x + 2, y + 2, fill_w, h - 4, fill);
    }
}

/* ==========================================================================
 * List box
 * ======================================================================= */
void gfx_list(int x, int y, int w, int h,
              const char **items, int count, int selected) {
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);
    uint32_t hi = gfx_theme_color(GFX_BG_HIGHLIGHT);
    uint32_t hifg = gfx_theme_color(GFX_FG_ACCENT);

    gfx_fill_rect(x, y, w, h, bg);
    gfx_bevel_in(x, y, w, h);

    int content_x = x + 4;
    int content_y = y + 4;
    int content_w = w - 8;
    int content_h = h - 8;

    int row_h = 20;
    int visible = content_h / row_h;
    int start = 0;
    if (selected >= visible) {
        start = selected - visible + 1;
    }

    gfx_set_clip(content_x, content_y, content_w, content_h);

    for (int i = 0; i < visible && (start + i) < count; i++) {
        int idx = start + i;
        int row_y = content_y + i * row_h;
        uint32_t row_bg = (idx == selected) ? hi : bg;
        uint32_t row_fg = (idx == selected) ? hifg : fg;

        gfx_fill_rect(content_x, row_y, content_w, row_h, row_bg);
        gfx_draw_text(content_x + 4, row_y + 6, items[idx], row_fg);
    }

    gfx_clear_clip();
}

/* ==========================================================================
 * Status / task bar
 * ======================================================================= */
void gfx_status_bar(int x, int y, int w, const char *text) {
    uint32_t bg = gfx_theme_color(GFX_BG_TITLE);
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT_DIM);

    gfx_fill_rect(x, y, w, 24, bg);
    gfx_bevel_out(x, y, w, 24);
    if (text) {
        gfx_draw_text(x + 4, y + 8, text, fg);
    }
}

/* ==========================================================================
 * Desktop background
 * ======================================================================= */
void gfx_desktop(void) {
    fb_clear(gfx_theme_color(GFX_BG_DESKTOP));
}

/* ==========================================================================
 * Your Design 2
 * ======================================================================= */
void gfx_logo_design2(int x, int y) {
    gfx_fill_rect(x, y, 150, 150, gfx_theme_color(GFX_RED));
    gfx_fill_rect(x + 25, y + 25, 150, 150, gfx_theme_color(GFX_GREEN));
    gfx_fill_rect(x + 50, y + 50, 150, 150, gfx_theme_color(GFX_BLUE));
    gfx_fill_rect(x + 50, y + 50, 125, 125, FB_RGBA(255, 0, 0, 255));
    gfx_draw_text(x, y-10, "Welcome to AmitX!", gfx_theme_color(GFX_WHITE));
}

/* ==========================================================================
 * Init
 * ======================================================================= */
void gfx_screen_init(void) {
    if (!fb.initialized) {
        printk("[gfx] FB not initialized\n");
        return;
    }
    gscreen.clip_enabled = 0;
    gscreen.clip_x = 0;
    gscreen.clip_y = 0;
    gscreen.clip_w = (int)fb.width;
    gscreen.clip_h = (int)fb.height;
    cursor_sx = -1;
    cursor_sy = -1;
    printk("[gfx] Screen layer ready (%ux%u)\n", fb.width, fb.height);
}

void gfx_draw_uptime(void) {
    if (!fb.initialized) return;
    
    int seconds = tick_count / 100;
    char buf[32];
    ksnprintf(buf, sizeof(buf), "Uptime: %ds", seconds);
    
    /* Draw in top-right corner with a small dark background */
    int tw = gfx_text_width(buf);
    int x = (int)fb.width - tw - 8;
    int y = 8;
    
    gfx_fill_rect(x - 4, y - 2, tw + 8, 12, gfx_theme_color(GFX_BG_PANEL));
    gfx_draw_text(x, y, buf, gfx_theme_color(GFX_FG_TEXT_DIM));
    
    /* Only present if we're not in a UI loop that will present anyway */
}