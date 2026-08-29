/*
 * gfx/compositor.c - Compositor
 * Author:   amity
 * Date:     Fri Jul  3 15:39:30 2026
 * Copyright © 2026 OwlyNest
 */

/* --- Styling Instructions ---
 * Encoding:                      UTF-8, Unix line endings
 * Text font:                     Monospace
 * Line width:                    Max 80 characters
 * Indentation:                   Use 4 spaces
 * Brace style:                   Same line as control statement
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

/* --- Macros ---*/

/* --- Includes ---*/
#include <gfx/compositor.h>
#include <gfx/fb.h>
#include <gfx/window.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <screen/printk.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static int compositor_dirty = 1;
/* --- Prototypes ---*/
static void compositor_clear_fb(void);
/* --- Functions ---*/

/* ==========================================================================
 * Render control
 * ======================================================================= */
void compositor_init(void) { compositor_dirty = 1; }

void compositor_request_update(void) { compositor_dirty = 1; }

int compositor_needs_update(void) { return compositor_dirty; }

/* ==========================================================================
 * Internal: clear framebuffer to desktop background
 * ======================================================================= */
static void compositor_clear_fb(void) {
  gfx_clear(&fb.back, gfx_theme_color(GFX_BG_DESKTOP));
  gfx_logo_os(&fb.back, 100, 100);
}

/* ==========================================================================
 * Blit a single window to the framebuffer
 * No z-order logic here — caller decides order
 * ======================================================================= */
void compositor_blit_window(window_t *win) {
  if (!win || !win->valid || !win->surface.pixels)
    return;
  if (win->state != WIN_STATE_VISIBLE)
    return;
  if (win->dirty == 0)
    return;

  int x0 = win->x;
  int y0 = win->y;

  for (uint32_t row = 0; row < win->surface.height; row++) {
    int fb_y = y0 + (int)row;
    if (fb_y < 0 || fb_y >= (int)fb.back.height)
      continue;

    uint32_t *src = win->surface.pixels + row * win->surface.pitch_px;
    uint32_t *dst = fb.back.pixels + fb_y * fb.back.pitch_px + x0;

    int copy_w = (int)win->surface.width;

    if (x0 < 0) {
      src += -x0;
      dst += -x0;
      copy_w += x0;
    }

    if (x0 + copy_w > (int)fb.back.width)
      copy_w = (int)fb.back.width - x0;

    if (copy_w <= 0)
      continue;

    memcpy(dst, src, (size_t)copy_w * sizeof(uint32_t));
  }
}

/* ==========================================================================
 * Full render: clear, composite all visible windows in z-order,
 * present to screen
 * ======================================================================= */
void compositor_render(void) {
  compositor_clear_fb();

  int count = window_get_z_count();
  for (int i = 0; i < count; i++) {
    window_handle_t h = window_get_z_at(i);
    window_t *win = window_get(h);
    if (win->dirty == 1) {
      compositor_blit_window(win);
    }
  }

  fb_present();
  compositor_dirty = 0;
}