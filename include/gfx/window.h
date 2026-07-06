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
#ifndef __GFX_WINDOW_H__
#define __GFX_WINDOW_H__

/* --- Window properties --- */
#define WIN_MAX_TITLE_LEN   64
#define WIN_MAX_WINDOWS     16

/* --- Window flags --- */
#define WIN_FLAG_NONE       0x00
#define WIN_FLAG_BORDER     0x01   /* Draw 3D border */
#define WIN_FLAG_TITLEBAR   0x02   /* Draw title bar */
#define WIN_FLAG_RESIZABLE  0x04   /* Allow resize (future) */
#define WIN_FLAG_MODAL      0x08   /* Blocks other windows (future) */

#define WIN_INVALID         (-1)

/* --- Handle encoding --- */
#define WIN_HANDLE_GEN_BITS  16
#define WIN_HANDLE_ID_MASK   0xFFFF

#define WIN_HANDLE_MAKE(id, gen)  \
    (((uint32_t)(gen) << WIN_HANDLE_GEN_BITS) | ((uint32_t)(id) & WIN_HANDLE_ID_MASK))
#define WIN_HANDLE_ID(h)          ((h) & WIN_HANDLE_ID_MASK)
#define WIN_HANDLE_GEN(h)         ((h) >> WIN_HANDLE_GEN_BITS)

/* --- Includes ---*/
#include <stdint.h>
#include <gfx/fb.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Window handle --- */
typedef uint32_t window_handle_t;

/* --- Window state --- */
typedef enum {
    WIN_STATE_HIDDEN,
    WIN_STATE_VISIBLE,
    WIN_STATE_FOCUSED,
    WIN_STATE_MINIMIZED,   // later
    WIN_STATE_DESTROYED
} win_state_t;

/* --- Window structure --- */
typedef struct {
    /* Identity and lifetime */
    uint32_t        id;
    uint32_t        generation;
    int             valid;

    /* State machine */
    win_state_t     state;
    int             focused;

    /* Position on the desktop */
    int             x;
    int             y;

    /* Drawing surface */
    gfx_surface_t   surface;

    /* Client area (relative to surface origin) */
    int             content_x;
    int             content_y;
    int             content_w;
    int             content_h;

    /* Metadata */
    char            title[WIN_MAX_TITLE_LEN];
    uint32_t        flags;
    uint32_t        fg_color;
    uint32_t        bg_color;

    /* Damage tracking */
    int             dirty;
} window_t;

/* --- Globals ---*/

/* --- Prototypes ---*/

/* ==========================================================================
 * Lifecycle
 * ======================================================================= */
window_handle_t window_create(int x, int y, int w, int h, const char *title, uint32_t flags);
void window_destroy(window_handle_t handle);

/* ==========================================================================
 * Access and validation
 * ======================================================================= */
window_t *window_get(window_handle_t handle);
int window_is_valid(window_handle_t handle);

/* ==========================================================================
 * Geometry
 * ======================================================================= */
void window_move(window_handle_t handle, int x, int y);
void window_resize(window_handle_t handle, int w, int h);

/* ==========================================================================
 * State machine
 * ======================================================================= */
void window_show(window_handle_t handle);
void window_hide(window_handle_t handle);
void window_focus(window_handle_t handle);
window_handle_t window_get_focused(void);
win_state_t window_get_state(window_handle_t handle);

/* ==========================================================================
 * Z-order and input routing
 * ======================================================================= */
window_handle_t window_at(int x, int y);
void window_raise(window_handle_t handle);
int window_get_z_order(window_handle_t handle);
int window_get_z_count(void);
window_handle_t window_get_z_top(void);
window_handle_t window_get_z_at(int index);

/* ==========================================================================
 * Content access
 * ======================================================================= */
uint32_t *window_pixels(window_handle_t handle);
int window_pitch(window_handle_t handle);

/* ==========================================================================
 * Drawing to window surface (clipped to window bounds)
 * ======================================================================= */
void window_clear(window_handle_t handle, uint32_t color);
void window_put_pixel(window_handle_t handle, uint32_t x, uint32_t y, uint32_t color);
void window_fill_rect(window_handle_t handle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void window_draw_text(window_handle_t handle, uint32_t x, uint32_t y, const char *str, uint32_t color);

/* ==========================================================================
 * Window chrome (frame, title bar)
 * ======================================================================= */
void window_draw_frame(window_handle_t handle);
void window_set_title(window_handle_t handle, const char *title);

/* ==========================================================================
 * Delegated drawing primitives
 * ======================================================================= */
void window_draw_line(window_handle_t handle, int x0, int y0, int x1, int y1,  uint32_t color);
void window_draw_line_thick(window_handle_t handle, int x0, int y0, int x1, int y1, int thickness, uint32_t color);
void window_draw_vector(window_handle_t handle, int x0, int y0, int angle, int magnitude, int thickness, uint32_t color);
void window_draw_circle(window_handle_t handle, int cx, int cy, int radius, uint32_t color);
void window_fill_circle(window_handle_t handle, int cx, int cy, int radius, uint32_t color);
void window_draw_arc(window_handle_t handle, int cx, int cy, int radius, int start_angle, int end_angle, uint32_t color);
void window_fill_sector(window_handle_t handle, int cx, int cy, int radius, int start_angle, int end_angle, uint32_t color);
void window_fill_triangle(window_handle_t handle, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);

#endif