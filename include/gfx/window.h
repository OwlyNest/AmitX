/*
	* include/gfx/window.h - Window abstraction
	* Author:   amity
	* Date:     Thu Jul  2 01:02:31 2026
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
#ifndef WINDOW_H
#define WINDOW_H

/* --- Window properties --- */
#define WIN_MAX_TITLE_LEN  64
#define WIN_MAX_WINDOWS    16

/* --- Window flags --- */
#define WIN_FLAG_NONE       0x00
#define WIN_FLAG_BORDER     0x01   /* Draw 3D border */
#define WIN_FLAG_TITLEBAR   0x02   /* Draw title bar */
#define WIN_FLAG_RESIZABLE  0x04   /* Allow resize (future) */
#define WIN_FLAG_MODAL      0x08   /* Blocks other windows (future) */

#define WIN_INVALID  (-1)
/* --- Includes ---*/
#include <stdint.h>
#include <gfx/fb.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Window handle --- */
typedef uint32_t window_handle_t;

/* --- Window state --- */
typedef struct {
    int valid;

    /* Position on the desktop */
    int x;
    int y;

    /* Drawing surface */
    gfx_surface_t surface;

    /* Client area */
    int content_x;
    int content_y;
    int content_w;
    int content_h;

    char title[WIN_MAX_TITLE_LEN];
    uint32_t flags;

    uint32_t fg_color;
    uint32_t bg_color;
} window_t;

/* --- Globals ---*/

/* --- Prototypes ---*/
/* --- Lifecycle --- */
window_handle_t window_create(int x, int y, int w, int h, const char *title, uint32_t flags);
void window_destroy(window_handle_t handle);

/* --- Geometry --- */
window_t *window_get(window_handle_t handle);
void window_move(window_handle_t handle, int x, int y);
void window_resize(window_handle_t handle, int w, int h);

/* --- Content access --- */
uint32_t *window_pixels(window_handle_t handle);
int window_pitch(window_handle_t handle);

/* --- Drawing to window surface --- */
void window_clear(window_handle_t handle, uint32_t color);
void window_put_pixel(window_handle_t handle, uint32_t x, uint32_t y, uint32_t color);
void window_fill_rect(window_handle_t handle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void window_draw_text(window_handle_t handle, uint32_t x, uint32_t y, const char *str, uint32_t color);

/* --- Window chrome (frame, title bar) --- */
void window_draw_frame(window_handle_t handle);
void window_set_title(window_handle_t handle, const char *title);

/* --- Compositing --- */
void window_present(window_handle_t handle);  /* Composite this window to framebuffer */
void window_present_all(void);                /* Composite all windows */

#endif