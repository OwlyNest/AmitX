/*
	* include/drivers/gfx_screen.h - [Enter description]
	* Author:   amity
	* Date:     Wed Jun 24 16:55:54 2026
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
#ifndef GFX_SCREEN_H
#define GFX_SCREEN_H
/* --- Includes ---*/
#include <stdint.h>
#include <drivers/fb.h>
/* --- Typedefs - Structs - Enums ---*/
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

/* --- Screen context ---*/
typedef struct gfx_screen {
    int clip_enabled;
    int clip_x, clip_y, clip_w, clip_h;
} gfx_screen_t;

/* --- Globals ---*/
extern gfx_screen_t gscreen;
/* --- Prototypes ---*/

/* --- Prototypes ---*/
uint32_t gfx_theme_color(gfx_theme_color_t c);

void gfx_screen_init(void);
void gfx_desktop(void);

/* Clipping */
void gfx_set_clip(int x, int y, int w, int h);
void gfx_clear_clip(void);

/* Primitives */
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color);
void gfx_hline(int x, int y, int w, uint32_t color);
void gfx_vline(int x, int y, int h, uint32_t color);

/* Text */
void gfx_draw_text(int x, int y, const char *str, uint32_t color);
int  gfx_text_width(const char *str);

/* Widgets */
void gfx_panel(int x, int y, int w, int h, uint32_t bg);
void gfx_bevel_in(int x, int y, int w, int h);
void gfx_bevel_out(int x, int y, int w, int h);
void gfx_title_bar(int x, int y, int w, const char *title);
void gfx_button(int x, int y, int w, int h, const char *label, int pressed);
void gfx_progress_bar(int x, int y, int w, int h, int percent, uint32_t fill, uint32_t empty);
void gfx_list(int x, int y, int w, int h, const char **items, int count, int selected);
void gfx_status_bar(int x, int y, int w, const char *text);

/* Mouse cursor */
void gfx_draw_cursor(int x, int y);
void gfx_cursor_show(int x, int y);
void gfx_cursor_hide(void);

/* Logo */
void gfx_logo_design2(int x, int y);

#endif