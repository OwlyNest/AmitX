/*
 * gfx/window.c - Window manager
 * Author:   amity
 * Date:     Thu Jul  2 01:02:27 2026
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
#include <gfx/font.h>
#include <gfx/window.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static window_t windows[WIN_MAX_WINDOWS];
static window_handle_t z_order[WIN_MAX_WINDOWS];
static int z_count = 0;
static uint32_t next_generation = 1;

/* --- Prototypes ---*/
static void window_update_content_rect(window_t *win);
static int window_find_free(void);
static void z_order_remove(window_handle_t handle);
static void z_order_push_top(window_handle_t handle);
static int z_order_index(window_handle_t handle);

/* --- Functions ---*/

/* ==========================================================================
 * Z-order stack (private)
 * ======================================================================= */
static int z_order_index(window_handle_t handle) {
  for (int i = 0; i < z_count; i++) {
    if (z_order[i] == handle)
      return i;
  }
  return -1;
}

static void z_order_remove(window_handle_t handle) {
  int idx = z_order_index(handle);
  if (idx < 0)
    return;

  for (int i = idx; i < z_count - 1; i++) {
    z_order[i] = z_order[i + 1];
  }
  z_count--;
}

static void z_order_push_top(window_handle_t handle) {
  if (z_count >= WIN_MAX_WINDOWS)
    return;
  z_order[z_count++] = handle;
}

/* ==========================================================================
 * Internal helpers
 * ======================================================================= */
static void window_update_content_rect(window_t *win) {
  int border = (win->flags & WIN_FLAG_BORDER) ? 2 : 0;
  int title_h = (win->flags & WIN_FLAG_TITLEBAR) ? 20 : 0;

  win->content_x = border;
  win->content_y = title_h + border;
  win->content_w = win->surface.width - (border * 2);
  win->content_h = win->surface.height - title_h - (border * 2);

  if (win->content_w < 0)
    win->content_w = 0;
  if (win->content_h < 0)
    win->content_h = 0;
}

static int window_find_free(void) {
  for (int i = 0; i < WIN_MAX_WINDOWS; i++) {
    if (!windows[i].valid)
      return i;
  }
  return -1;
}

/* ==========================================================================
 * Lifecycle
 * ======================================================================= */
window_handle_t window_create(int x, int y, int w, int h, const char *title,
                              uint32_t flags) {
  int slot = window_find_free();
  if (slot < 0) {
    printk("[window] No free window slots\n");
    return WIN_INVALID;
  }

  window_t *win = &windows[slot];
  memset(win, 0, sizeof(window_t));

  win->id = (uint32_t)slot;
  win->generation = next_generation++;
  win->valid = 1;

  win->state = WIN_STATE_VISIBLE;
  win->focused = 0;

  win->x = x;
  win->y = y;

  win->surface.width = w;
  win->surface.height = h;

  win->flags = flags;
  win->fg_color = gfx_theme_color(GFX_WHITE);
  win->bg_color = gfx_theme_color(GFX_BG_PANEL);
  win->dirty = 1;

  if (title) {
    strncpy(win->title, title, WIN_MAX_TITLE_LEN - 1);
    win->title[WIN_MAX_TITLE_LEN - 1] = '\0';
  }

  window_update_content_rect(win);

  /* Allocate pixel buffer */
  win->surface.pitch_px = win->surface.width;
  win->surface.pitch = w * sizeof(uint32_t);
  size_t buf_size =
      (size_t)win->surface.width * win->surface.height * sizeof(uint32_t);
  win->surface.pixels = (uint32_t *)malloc(buf_size);
  if (!win->surface.pixels) {
    win->valid = 0;
    return WIN_INVALID;
  }

  /* Clear to background */
  window_clear(slot, win->bg_color);

  /* Draw chrome */
  if (flags & (WIN_FLAG_BORDER | WIN_FLAG_TITLEBAR)) {
    window_draw_frame(slot);
  }

  window_handle_t handle = WIN_HANDLE_MAKE(slot, win->generation);
  z_order_push_top(handle);

  printk("[window] Created window %d (gen %u): '%s' at (%d,%d) %dx%d\n", slot,
         win->generation, win->title, x, y, w, h);

  return handle;
}

void window_destroy(window_handle_t handle) {
  uint32_t slot = WIN_HANDLE_ID(handle);
  if (slot >= WIN_MAX_WINDOWS)
    return;

  window_t *win = &windows[slot];
  if (!win->valid || win->generation != WIN_HANDLE_GEN(handle)) {
    return;
  }

  z_order_remove(handle);

  if (win->surface.pixels) {
    free(win->surface.pixels);
    win->surface.pixels = NULL;
  }

  win->valid = 0;
  win->generation = 0;

  printk("[window] Destroyed window %d\n", slot);
}

/* ==========================================================================
 * Access and validation
 * ======================================================================= */
window_t *window_get(window_handle_t handle) {
  uint32_t slot = WIN_HANDLE_ID(handle);
  if (slot >= WIN_MAX_WINDOWS)
    return NULL;

  window_t *win = &windows[slot];
  if (!win->valid || win->generation != WIN_HANDLE_GEN(handle)) {
    return NULL;
  }
  return win;
}

int window_is_valid(window_handle_t handle) {
  return window_get(handle) != NULL;
}

/* ==========================================================================
 * Geometry
 * ======================================================================= */
void window_move(window_handle_t handle, int x, int y) {
  window_t *win = window_get(handle);
  if (!win)
    return;

  win->x = x;
  win->y = y;
  win->dirty = 1;
}

void window_resize(window_handle_t handle, int w, int h) {
  window_t *win = window_get(handle);
  if (!win)
    return;

  if (win->surface.pixels)
    free(win->surface.pixels);

  win->surface.width = w;
  win->surface.height = h;
  win->surface.pitch_px = w;

  size_t buf_size = (size_t)w * h * sizeof(uint32_t);
  win->surface.pixels = (uint32_t *)malloc(buf_size);

  window_update_content_rect(win);
  window_clear(handle, win->bg_color);

  if (win->flags & (WIN_FLAG_BORDER | WIN_FLAG_TITLEBAR)) {
    window_draw_frame(handle);
  }

  win->dirty = 1;
}

/* ==========================================================================
 * State machine
 * ======================================================================= */
void window_show(window_handle_t handle) {
  window_t *win = window_get(handle);
  if (!win)
    return;
  if (win->state == WIN_STATE_VISIBLE)
    return;

  win->state = WIN_STATE_VISIBLE;
  win->dirty = 1;

  /* Add back to z-order if not present */
  if (z_order_index(handle) < 0) {
    z_order_push_top(handle);
  }
}

void window_hide(window_handle_t handle) {
  window_t *win = window_get(handle);
  if (!win)
    return;
  if (win->state != WIN_STATE_VISIBLE)
    return;

  win->state = WIN_STATE_HIDDEN;
  win->focused = 0;
  win->dirty = 1;

  z_order_remove(handle);
}

void window_focus(window_handle_t handle) {
  window_t *target = window_get(handle);
  if (!target)
    return;
  if (target->state != WIN_STATE_VISIBLE)
    return;

  /* Unfocus all */
  for (int i = 0; i < WIN_MAX_WINDOWS; i++) {
    if (windows[i].valid) {
      windows[i].focused = 0;
    }
  }

  target->focused = 1;

  /* Bring to top */
  z_order_remove(handle);
  z_order_push_top(handle);

  target->dirty = 1;
}

window_handle_t window_get_focused(void) {
  for (int i = z_count - 1; i >= 0; i--) {
    window_t *win = window_get(z_order[i]);
    if (win && win->focused && win->state == WIN_STATE_VISIBLE) {
      return z_order[i];
    }
  }
  return WIN_INVALID;
}

win_state_t window_get_state(window_handle_t handle) {
  window_t *win = window_get(handle);
  return win ? win->state : WIN_STATE_HIDDEN;
}

/* ==========================================================================
 * Z-order and input routing
 * ======================================================================= */
window_handle_t window_at(int x, int y) {
  /* Search from top (end of z_order) to bottom */
  for (int i = z_count - 1; i >= 0; i--) {
    window_t *win = window_get(z_order[i]);
    if (!win)
      continue;
    if (win->state != WIN_STATE_VISIBLE)
      continue;

    int x0 = win->x;
    int y0 = win->y;
    int x1 = x0 + (int)win->surface.width;
    int y1 = y0 + (int)win->surface.height;

    if (x >= x0 && x < x1 && y >= y0 && y < y1) {
      return z_order[i];
    }
  }
  return WIN_INVALID;
}

void window_raise(window_handle_t handle) {
  window_t *win = window_get(handle);
  if (!win)
    return;
  if (win->state != WIN_STATE_VISIBLE)
    return;

  z_order_remove(handle);
  z_order_push_top(handle);
  win->dirty = 1;
}

int window_get_z_order(window_handle_t handle) { return z_order_index(handle); }

int window_get_z_count(void) { return z_count; }

window_handle_t window_get_z_top(void) {
  if (z_count <= 0)
    return WIN_INVALID;
  return z_order[z_count - 1];
}

window_handle_t window_get_z_at(int index) {
  if (index < 0 || index >= z_count)
    return WIN_INVALID;
  return z_order[index];
}

/* ==========================================================================
 * Content access
 * ======================================================================= */
uint32_t *window_pixels(window_handle_t handle) {
  window_t *win = window_get(handle);
  return win ? win->surface.pixels : NULL;
}

int window_pitch(window_handle_t handle) {
  window_t *win = window_get(handle);
  return win ? win->surface.pitch_px : 0;
}

/* ==========================================================================
 * Drawing to window surface (clipped to window bounds)
 * ======================================================================= */
void window_clear(window_handle_t handle, uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;

  for (uint32_t row = 0; row < win->surface.height; row++) {
    uint32_t *dest = win->surface.pixels + row * win->surface.pitch_px;
    for (uint32_t col = 0; col < win->surface.width; col++) {
      dest[col] = color;
    }
  }
  win->dirty = 1;
}

void window_put_pixel(window_handle_t handle, uint32_t x, uint32_t y,
                      uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;
  if (x >= win->surface.width || y >= win->surface.height)
    return;

  win->surface.pixels[y * win->surface.pitch_px + x] = color;
  win->dirty = 1;
}

void window_fill_rect(window_handle_t handle, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;

  if (x + w > win->surface.width)
    w = win->surface.width - x;
  if (y + h > win->surface.height)
    h = win->surface.height - y;
  if (x >= win->surface.width || y >= win->surface.height || w == 0 || h == 0)
    return;

  for (uint32_t row = y; row < y + h; row++) {
    uint32_t *dest = win->surface.pixels + row * win->surface.pitch_px + x;
    for (uint32_t col = 0; col < w; col++) {
      dest[col] = color;
    }
  }
  win->dirty = 1;
}

void window_draw_text(window_handle_t handle, uint32_t x, uint32_t y,
                      const char *str, uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels || !str)
    return;

  uint32_t cx = x;
  uint32_t cy = y;

  while (*str) {
    if (*str == '\n') {
      cx = x;
      cy += 8;
      str++;
      continue;
    }
    if (cx + 8 <= win->surface.width && cy + 8 <= win->surface.height) {
      gfx_draw_char(&win->surface, cx, cy, *str, color);
    }
    cx += 8;
    str++;
  }
  win->dirty = 1;
}

/* ==========================================================================
 * Window chrome (frame, title bar)
 * ======================================================================= */
void window_draw_frame(window_handle_t handle) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;

  uint32_t light = gfx_theme_color(GFX_BORDER_LIGHT);
  uint32_t dark = gfx_theme_color(GFX_BORDER_DARK);
  uint32_t title_bg = gfx_theme_color(GFX_BG_TITLE);

  int w = win->surface.width;
  int h = win->surface.height;

  if (win->flags & WIN_FLAG_BORDER) {
    /* Top */
    for (int i = 0; i < w; i++) {
      win->surface.pixels[i] = light;
      win->surface.pixels[(h - 1) * win->surface.pitch_px + i] = dark;
    }
    /* Sides */
    for (int i = 0; i < h; i++) {
      win->surface.pixels[i * win->surface.pitch_px] = light;
      win->surface.pixels[i * win->surface.pitch_px + (w - 1)] = dark;
    }
  }

  if (win->flags & WIN_FLAG_TITLEBAR) {
    int title_h = 20;
    for (int row = 0; row < title_h && row < h; row++) {
      for (int col = 0; col < w; col++) {
        win->surface.pixels[row * win->surface.pitch_px + col] = title_bg;
      }
    }
    /* Draw title text */
    int title_len = strlen(win->title);
    int tx = 4;
    int ty = 6;
    for (int i = 0; i < title_len && (tx + i * 8) < w - 8; i++) {
      gfx_draw_char(&win->surface, tx + i * 8, ty, win->title[i],
                    gfx_theme_color(GFX_WHITE));
    }
  }
  win->dirty = 1;
}

void window_set_title(window_handle_t handle, const char *title) {
  window_t *win = window_get(handle);
  if (!win)
    return;
  if (title) {
    strncpy(win->title, title, WIN_MAX_TITLE_LEN - 1);
    win->title[WIN_MAX_TITLE_LEN - 1] = '\0';
  }
  if (win->flags & WIN_FLAG_TITLEBAR) {
    window_draw_frame(handle);
  }
}

/* ==========================================================================
 * Delegated drawing primitives
 * ======================================================================= */
void window_draw_line(window_handle_t handle, int x0, int y0, int x1, int y1,
                      uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;
  gfx_draw_line(&win->surface, x0, y0, x1, y1, color);
  win->dirty = 1;
}

void window_draw_line_thick(window_handle_t handle, int x0, int y0, int x1,
                            int y1, int thickness, uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;
  gfx_draw_line_thick(&win->surface, x0, y0, x1, y1, thickness, color);
  win->dirty = 1;
}

void window_draw_vector(window_handle_t handle, int x0, int y0, int angle,
                        int magnitude, int thickness, uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;
  gfx_draw_vector(&win->surface, x0, y0, angle, magnitude, thickness, color);
  win->dirty = 1;
}

void window_draw_circle(window_handle_t handle, int cx, int cy, int radius,
                        uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;
  gfx_draw_circle(&win->surface, cx, cy, radius, color);
  win->dirty = 1;
}

void window_fill_circle(window_handle_t handle, int cx, int cy, int radius,
                        uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;
  gfx_fill_circle(&win->surface, cx, cy, radius, color);
  win->dirty = 1;
}

void window_draw_arc(window_handle_t handle, int cx, int cy, int radius,
                     int start_angle, int end_angle, uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;
  gfx_draw_arc(&win->surface, cx, cy, radius, start_angle, end_angle, color);
  win->dirty = 1;
}

void window_fill_sector(window_handle_t handle, int cx, int cy, int radius,
                        int start_angle, int end_angle, uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;
  gfx_fill_sector(&win->surface, cx, cy, radius, start_angle, end_angle, color);
  win->dirty = 1;
}

void window_fill_triangle(window_handle_t handle, int x0, int y0, int x1,
                          int y1, int x2, int y2, uint32_t color) {
  window_t *win = window_get(handle);
  if (!win || !win->surface.pixels)
    return;
  gfx_fill_triangle(&win->surface, x0, y0, x1, y1, x2, y2, color);
  win->dirty = 1;
}