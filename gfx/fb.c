/*
 * gfx/fb.c - framebuffer and 2d graphics
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
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

/* --- Macros ---*/

/* --- Includes ---*/
#include <drivers/mouse.h>
#include <gfx/fb.h>
#include <gfx/font.h>
#include <hw/svga.h>
#include <internal/virtmem.h>
#include <lib/math.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <mm/mmap.h>
#include <mm/paging.h>
#include <mm/pmm.h>
#include <screen/printk.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
fb_surface_t fb = {0};
static int fb_back_owned = 0;
/* --- Prototypes ---*/

/* --- Functions ---*/
/* ==========================================================================
 * Initialize Framebuffer
 * ======================================================================= */

/* Hardware properties of whatever is driving the screen */
static struct {
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t bpp;
  uint32_t red_mask;
  uint32_t green_mask;
  uint32_t blue_mask;
  int is_mmio;
} fb_hw = {0};

#define FB_VIRT_BASE 0xDF000000u

static void fb_compute_masks(const framebuffer_info_t *bi) {
  if (bi->type == 1 && bi->bpp >= 15) {
    fb_hw.red_mask = ((1u << bi->red_size) - 1) << bi->red_pos;
    fb_hw.green_mask = ((1u << bi->green_size) - 1) << bi->green_pos;
    fb_hw.blue_mask = ((1u << bi->blue_size) - 1) << bi->blue_pos;
  } else {
    fb_hw.red_mask = 0x00FF0000;
    fb_hw.green_mask = 0x0000FF00;
    fb_hw.blue_mask = 0x000000FF;
  }
}

/* ------------------------------------------------------------------ */
int fb_init(void) {
  const boot_info_t *bi = pmm_get_boot_info();

  /* Bootloader framebuffer addresses are physical until paging is ready. */
  if (!paging_enabled())
    return -1;

  if (fb_back_owned && fb.back.pixels) {
    free(fb.back.pixels);
    fb.back.pixels = NULL;
  }
  fb_back_owned = 0;

  /* --- SVGA-II backend (preferred) --- */
  if (svga.initialized && svga.fb_virt) {
    fb_hw.width = svga.width;
    fb_hw.height = svga.height;
    fb_hw.pitch = svga.pitch;
    fb_hw.bpp = svga.bpp;
    fb_hw.red_mask = svga.red_mask;
    fb_hw.green_mask = svga.green_mask;
    fb_hw.blue_mask = svga.blue_mask;
    fb.front = (uint32_t *)svga.fb_virt;

    /* --- Bootloader framebuffer (UEFI GOP, VBE, etc.) --- */
  } else if (bi && bi->fb.valid) {
    fb_hw.width = bi->fb.width;
    fb_hw.height = bi->fb.height;
    fb_hw.pitch = bi->fb.pitch;
    fb_hw.bpp = bi->fb.bpp;
    fb_compute_masks(&bi->fb);

    uintptr_t fb_phys = (uintptr_t)bi->fb.addr;
    size_t fb_size = (size_t)fb_hw.pitch * fb_hw.height;

    if (paging_enabled()) {
      uintptr_t phys_start = fb_phys & ~(uintptr_t)(PAGE_SIZE - 1);
      uintptr_t phys_offset = fb_phys - phys_start;
      size_t map_size = fb_size + phys_offset;

      for (size_t off = 0; off < map_size; off += PAGE_SIZE) {
        if (map_page(phys_start + off, FB_VIRT_BASE + off,
                     PAGE_WRITABLE | PAGE_NOCACHE) != 0) {
          printk("[fb] map_page failed for FB\n");
          return -1;
        }
      }
      fb.front = (uint32_t *)(FB_VIRT_BASE + phys_offset);
    } else {
      fb.front = (uint32_t *)fb_phys;
    }

  } else {
    printk("[fb] No framebuffer backend available\n");
    return -1;
  }

  /* --- Backbuffer --- */
  fb.back.width = fb_hw.width;
  fb.back.height = fb_hw.height;

  size_t buf_size = (size_t)fb.back.width * fb.back.height * sizeof(uint32_t);
  fb.back.pixels = heap_base ? (uint32_t *)malloc(buf_size) : NULL;

  if (fb.back.pixels) {
    fb.back.pitch = fb.back.width * sizeof(uint32_t);
    fb.back.pitch_px = fb.back.width;
    fb_back_owned = 1;
    memset(fb.back.pixels, 0, buf_size);
  } else {
    /* Heap not ready yet (early boot) — draw direct to frontbuffer */
    fb.back.pixels = fb.front;
    fb.back.pitch = fb_hw.pitch;
    fb.back.pitch_px = fb_hw.pitch / (fb_hw.bpp / 8);
  }

  fb.initialized = 1;
  return 0;
}

void fb_update_hw(void) {
  if (svga.initialized && svga.fb_virt) {
    fb_hw.width = svga.width;
    fb_hw.height = svga.height;
    fb_hw.pitch = svga.pitch;
    fb_hw.bpp = svga.bpp;
    fb_hw.red_mask = svga.red_mask;
    fb_hw.green_mask = svga.green_mask;
    fb_hw.blue_mask = svga.blue_mask;
    fb.front = (uint32_t *)svga.fb_virt;
  }
}

/* ==========================================================================
 * Present backbuffer to screen
 * ======================================================================= */
void fb_present(void) {
  if (!fb.initialized)
    return;

  if (fb_hw.bpp == 32 && fb.back.pitch == fb_hw.pitch) {
    memcpy(fb.front, fb.back.pixels, fb.back.pitch * fb.back.height);
  } else {
    for (uint32_t y = 0; y < fb.back.height; y++) {
      uint32_t *src_row = fb.back.pixels + y * fb.back.pitch_px;
      uint8_t *dst_row = (uint8_t *)fb.front + y * fb_hw.pitch;

      for (uint32_t x = 0; x < fb.back.width; x++) {
        uint32_t color = src_row[x];
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        if (fb_hw.bpp == 24) {
          dst_row[x * 3 + 0] = b;
          dst_row[x * 3 + 1] = g;
          dst_row[x * 3 + 2] = r;
        } else if (fb_hw.bpp == 16) {
          uint16_t *dst16 = (uint16_t *)dst_row;
          dst16[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
      }
    }
  }

  if (svga.initialized)
    svga_update_full();
  mouse_refresh_cursor();
}

uint32_t fb_pack_pixel(uint8_t r, uint8_t g, uint8_t b) {
  if (fb_hw.bpp == 32) {
    if (fb_hw.red_mask == 0x00FF0000) {
      return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    } else if (fb_hw.red_mask == 0x000000FF) {
      return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    }
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }
  if (fb_hw.bpp == 24) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }
  if (fb_hw.bpp == 16) {
    uint32_t r5 = (r >> 3) & 0x1F;
    uint32_t g6 = (g >> 2) & 0x3F;
    uint32_t b5 = (b >> 3) & 0x1F;
    return (r5 << 11) | (g6 << 5) | b5;
  }
  if (fb_hw.bpp == 15) {
    uint32_t r5 = (r >> 3) & 0x1F;
    uint32_t g5 = (g >> 3) & 0x1F;
    uint32_t b5 = (b >> 3) & 0x1F;
    return (r5 << 10) | (g5 << 5) | b5;
  }
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/* ==========================================================================
 * Color helpers
 * ======================================================================= */
uint32_t gfx_theme_color(gfx_theme_color_t c) {
  switch (c) {
  case GFX_BG_DESKTOP:
    return fb_pack_pixel(20, 30, 100);
  case GFX_BG_PANEL:
    return fb_pack_pixel(40, 45, 60);
  case GFX_BG_TITLE:
    return fb_pack_pixel(80, 90, 120);
  case GFX_BG_HIGHLIGHT:
    return fb_pack_pixel(100, 120, 160);
  case GFX_BG_BUTTON:
    return fb_pack_pixel(60, 70, 90);
  case GFX_BG_BUTTON_HOVER:
    return fb_pack_pixel(80, 95, 120);
  case GFX_FG_TEXT:
    return fb_pack_pixel(255, 255, 255);
  case GFX_FG_TEXT_DIM:
    return fb_pack_pixel(180, 180, 200);
  case GFX_FG_ACCENT:
    return fb_pack_pixel(100, 200, 255);
  case GFX_BORDER_LIGHT:
    return fb_pack_pixel(120, 130, 150);
  case GFX_BORDER_DARK:
    return fb_pack_pixel(20, 25, 35);
  case GFX_RED:
    return fb_pack_pixel(255, 0, 0);
  case GFX_GREEN:
    return fb_pack_pixel(0, 255, 0);
  case GFX_BLUE:
    return fb_pack_pixel(0, 0, 255);
  case GFX_YELLOW:
    return fb_pack_pixel(255, 255, 0);
  case GFX_WHITE:
    return fb_pack_pixel(255, 255, 255);
  case GFX_BLACK:
    return fb_pack_pixel(0, 0, 0);
  default:
    return fb_pack_pixel(255, 255, 255);
  }
}

/* ==========================================================================
 * Basic Drawing
 * ======================================================================= */
void fb_clear(uint32_t color) {
  if (!fb.initialized)
    return;
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
  if (!fb.initialized)
    return;
  gfx_put_pixel(&fb.back, x, y, color);
}
void gfx_put_pixel(gfx_surface_t *surface, uint32_t x, uint32_t y,
                   uint32_t color) {
  if (x >= surface->width || y >= surface->height)
    return;
  surface->pixels[y * surface->pitch_px + x] = color;
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                  uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_fill_rect(&fb.back, x, y, w, h, color);
}

void gfx_fill_rect(gfx_surface_t *surface, uint32_t x, uint32_t y, uint32_t w,
                   uint32_t h, uint32_t color) {
  if (x + w > surface->width)
    w = surface->width - x;
  if (y + h > surface->height)
    h = surface->height - y;
  if (x >= surface->width || y >= surface->height)
    return;

  for (uint32_t row = y; row < y + h; row++) {
    uint32_t *dest = surface->pixels + row * surface->pitch_px + x;
    for (uint32_t col = 0; col < w; col++) {
      dest[col] = color;
    }
  }
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                  uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_draw_rect(&fb.back, x, y, w, h, color);
}
void gfx_draw_rect(gfx_surface_t *surface, uint32_t x, uint32_t y, uint32_t w,
                   uint32_t h, uint32_t color) {
  gfx_draw_line(surface, x, y, x + w, y, color);
  gfx_draw_line(surface, x, y, x, y + h, color);
  gfx_draw_line(surface, x + w, y, x + w, y + h, color);
  gfx_draw_line(surface, x, y + h, x + w, y + h, color);
}

/* ==========================================================================
 * Bresenham's Line Algorithm
 * ======================================================================= */
static inline int abs(int x) { return x < 0 ? -x : x; }

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_draw_line(&fb.back, x0, y0, x1, y1, color);
}

void gfx_draw_line(gfx_surface_t *surface, int x0, int y0, int x1, int y1,
                   uint32_t color) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2;

  while (1) {
    gfx_put_pixel(surface, x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void gfx_hline(gfx_surface_t *surface, int x, int y, int w, uint32_t color) {
  int h = 1;
  gfx_fill_rect(surface, (uint32_t)x, (uint32_t)y, (uint32_t)w, h, color);
}

void gfx_vline(gfx_surface_t *surface, int x, int y, int h, uint32_t color) {
  int w = 1;
  gfx_fill_rect(surface, (uint32_t)x, (uint32_t)y, w, (uint32_t)h, color);
}

/* ==========================================================================
 * Circle (midpoint algorithm)
 * ======================================================================= */
void fb_draw_circle(int cx, int cy, int radius, uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_draw_circle(&fb.back, cx, cy, radius, color);
}

void gfx_draw_circle(gfx_surface_t *surface, int cx, int cy, int radius,
                     uint32_t color) {
  if (radius <= 0)
    return;

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
  if (!fb.initialized)
    return;
  gfx_draw_char(&fb.back, x, y, c, color);
}
void gfx_draw_char(gfx_surface_t *surface, uint32_t x, uint32_t y, char c,
                   uint32_t color) {
  if (x >= surface->width || y >= surface->height)
    return;
  unsigned char uc = (unsigned char)c;
  if (uc < 32 || uc >= 128)
    uc = '?';

  const uint8_t *glyph = font8x8[uc];

  for (int row = 0; row < 8; row++) {
    if (y + row >= surface->height)
      break;
    uint8_t line = glyph[row];
    for (int col = 0; col < 8; col++) {
      if (x + col >= surface->width)
        break;
      if (line & (1u << (7 - col))) {
        gfx_put_pixel(surface, x + col, y + row, color);
      }
    }
  }
}

void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_draw_string(&fb.back, x, y, str, color);
}
void gfx_draw_string(gfx_surface_t *surface, uint32_t x, uint32_t y,
                     const char *str, uint32_t color) {
  if (!str)
    return;

  uint32_t cx = x;
  while (*str) {
    gfx_draw_char(surface, cx, y, *str++, color);
    cx += 8;
  }
}

/* ==========================================================================
 * Filled circle (scanline)
 * ======================================================================= */
void fb_fill_circle(int cx, int cy, int radius, uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_fill_circle(&fb.back, cx, cy, radius, color);
}

void gfx_fill_circle(gfx_surface_t *surface, int cx, int cy, int radius,
                     uint32_t color) {
  if (radius <= 0)
    return;

  int r_sq = radius * radius;

  for (int y = -radius; y <= radius; y++) {
    int row = cy + y;
    if (row < 0 || row >= (int)surface->height)
      continue;

    int x_len = (int)fx_to_int(fx_sqrt(fx_from_int(r_sq - y * y)));
    int x0 = cx - x_len;
    int x1 = cx + x_len;

    if (x0 < 0)
      x0 = 0;
    if (x1 >= (int)surface->width)
      x1 = surface->width - 1;

    if (x0 <= x1) {
      uint32_t *dest = surface->pixels + row * surface->pitch_px + x0;
      for (int x = x0; x <= x1; x++) {
        *dest++ = color;
      }
    }
  }
}

/* ==========================================================================
 * Thick line: draw a line with circular pen of given radius
 * Uses Bresenham + perpendicular fill
 * ======================================================================= */
void fb_draw_line_thick(int x0, int y0, int x1, int y1, int thickness,
                        uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_draw_line_thick(&fb.back, x0, y0, x1, y1, thickness, color);
}

void gfx_draw_line_thick(gfx_surface_t *surface, int x0, int y0, int x1, int y1,
                         int thickness, uint32_t color) {
  if (thickness <= 1) {
    gfx_draw_line(surface, x0, y0, x1, y1, color);
    return;
  }

  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx - dy;

  int r = thickness / 2;
  if (r < 1)
    r = 1;

  while (1) {
    /* Draw a filled circle at each pixel of the line */
    gfx_fill_circle(surface, x0, y0, r, color);

    if (x0 == x1 && y0 == y1)
      break;

    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

/* ==========================================================================
 * Vector: line from (x0,y0) at angle with magnitude
 * Angle: tenths of degrees, 0 = right (3 o'clock), CCW
 * ======================================================================= */
void fb_draw_vector(int x0, int y0, int angle, int magnitude, int thickness,
                    uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_draw_vector(&fb.back, x0, y0, angle, magnitude, thickness, color);
}

void gfx_draw_vector(gfx_surface_t *surface, int x0, int y0, int angle,
                     int magnitude, int thickness, uint32_t color) {
  vec2i_t end = vec2i_polar_bradians(x0, y0, magnitude, angle);
  gfx_draw_line_thick(surface, x0, y0, end.x, end.y, thickness, color);
}

/* ==========================================================================
 * Arc outline: draw arc from start_angle to end_angle (tenths of degrees)
 * ======================================================================= */
void fb_draw_arc(int cx, int cy, int radius, int start_angle, int end_angle,
                 uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_draw_arc(&fb.back, cx, cy, radius, start_angle, end_angle, color);
}

void gfx_draw_arc(gfx_surface_t *surface, int cx, int cy, int radius,
                  int start_angle, int end_angle, uint32_t color) {
  if (radius <= 0)
    return;

  /* Normalize angles */
  start_angle = start_angle % ANGLE_FULL_BRAD;
  if (start_angle < 0)
    start_angle += ANGLE_FULL_BRAD;
  end_angle = end_angle % ANGLE_FULL_BRAD;
  if (end_angle < 0)
    end_angle += ANGLE_FULL_BRAD;

  int step = 10; /* 1 degree steps, adjust for smoothness vs speed */
  if (radius > 100)
    step = 5;
  if (radius > 300)
    step = 2;

  int a = start_angle;
  int done = 0;

  while (!done) {
    vec2i_t p = vec2i_polar_bradians(cx, cy, radius, a);
    gfx_put_pixel(surface, p.x, p.y, color);

    if (a == end_angle) {
      done = 1;
    } else {
      a += step;
      if (a >= ANGLE_FULL_BRAD)
        a -= ANGLE_FULL_BRAD;
      /* If we would overshoot end_angle, snap to it */
      if (step > 0 &&
          ((end_angle > start_angle && a > end_angle) ||
           (end_angle < start_angle && a > end_angle && a < start_angle))) {
        a = end_angle;
      }
    }
  }
}

/* ==========================================================================
 * Sector fill (pie slice): fill area between two angles
 * ======================================================================= */
void fb_fill_sector(int cx, int cy, int radius, int start_angle, int end_angle,
                    uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_fill_sector(&fb.back, cx, cy, radius, start_angle, end_angle, color);
}

void gfx_fill_sector(gfx_surface_t *surface, int cx, int cy, int radius,
                     int start_angle, int end_angle, uint32_t color) {
  if (radius <= 0)
    return;

  start_angle = start_angle % ANGLE_FULL_BRAD;
  if (start_angle < 0)
    start_angle += ANGLE_FULL_BRAD;
  end_angle = end_angle % ANGLE_FULL_BRAD;
  if (end_angle < 0)
    end_angle += ANGLE_FULL_BRAD;

  int step = 15; /* 1.5 degree triangles */
  int a = start_angle;

  vec2i_t center = vec2i(cx, cy);
  vec2i_t prev = vec2i_polar_bradians(cx, cy, radius, a);

  while (1) {
    int next_a = a + step;
    if (next_a > end_angle && a != end_angle) {
      next_a = end_angle;
    } else if (a == end_angle) {
      break;
    }

    vec2i_t next = vec2i_polar_bradians(cx, cy, radius, next_a);
    gfx_fill_triangle(surface, center.x, center.y, prev.x, prev.y, next.x,
                      next.y, color);

    if (next_a == end_angle)
      break;
    a = next_a;
    prev = next;
  }
}

/* ==========================================================================
 * Triangle fill: decompose into flat-top + flat-bottom trapezoids
 * ======================================================================= */
static void gfx_fill_flat_top_triangle(gfx_surface_t *surface, int x0, int y0,
                                       int x1, int y1, int x2, int y2,
                                       uint32_t color) {
  /* y0 == y1, y2 is the bottom point */
  float inv_slope_0 = (float)(x2 - x0) / (float)(y2 - y0);
  float inv_slope_1 = (float)(x2 - x1) / (float)(y2 - y1);

  float x_start = (float)x0;
  float x_end = (float)x1;

  for (int y = y0; y <= y2; y++) {
    int x_s = (int)x_start;
    int x_e = (int)x_end;
    if (x_s > x_e) {
      int t = x_s;
      x_s = x_e;
      x_e = t;
    }

    if (y >= 0 && y < (int)surface->height) {
      if (x_s < 0)
        x_s = 0;
      if (x_e >= (int)surface->width)
        x_e = surface->width - 1;
      uint32_t *dest = surface->pixels + y * surface->pitch_px + x_s;
      for (int x = x_s; x <= x_e; x++) {
        *dest++ = color;
      }
    }

    x_start += inv_slope_0;
    x_end += inv_slope_1;
  }
}

static void gfx_fill_flat_bottom_triangle(gfx_surface_t *surface, int x0,
                                          int y0, int x1, int y1, int x2,
                                          int y2, uint32_t color) {
  /* y2 == y1, y0 is the top point */
  float inv_slope_0 = (float)(x1 - x0) / (float)(y1 - y0);
  float inv_slope_1 = (float)(x2 - x0) / (float)(y2 - y0);

  float x_start = (float)x0;
  float x_end = (float)x0;

  for (int y = y0; y <= y1; y++) {
    int x_s = (int)x_start;
    int x_e = (int)x_end;
    if (x_s > x_e) {
      int t = x_s;
      x_s = x_e;
      x_e = t;
    }

    if (y >= 0 && y < (int)surface->height) {
      if (x_s < 0)
        x_s = 0;
      if (x_e >= (int)surface->width)
        x_e = surface->width - 1;
      uint32_t *dest = surface->pixels + y * surface->pitch_px + x_s;
      for (int x = x_s; x <= x_e; x++) {
        *dest++ = color;
      }
    }

    x_start += inv_slope_0;
    x_end += inv_slope_1;
  }
}

void fb_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2,
                      uint32_t color) {
  if (!fb.initialized)
    return;
  gfx_fill_triangle(&fb.back, x0, y0, x1, y1, x2, y2, color);
}

void gfx_fill_triangle(gfx_surface_t *surface, int x0, int y0, int x1, int y1,
                       int x2, int y2, uint32_t color) {
  /* Sort by Y */
  if (y0 > y1) {
    int t;
    t = x0;
    x0 = x1;
    x1 = t;
    t = y0;
    y0 = y1;
    y1 = t;
  }
  if (y1 > y2) {
    int t;
    t = x1;
    x1 = x2;
    x2 = t;
    t = y1;
    y1 = y2;
    y2 = t;
  }
  if (y0 > y1) {
    int t;
    t = x0;
    x0 = x1;
    x1 = t;
    t = y0;
    y0 = y1;
    y1 = t;
  }

  if (y0 == y2)
    return; /* degenerate */

  if (y1 == y2) {
    /* Flat top */
    gfx_fill_flat_bottom_triangle(surface, x0, y0, x1, y1, x2, y2, color);
  } else if (y0 == y1) {
    /* Flat bottom */
    gfx_fill_flat_top_triangle(surface, x0, y0, x1, y1, x2, y2, color);
  } else {
    /* Split into flat-bottom + flat-top */
    int x3 = x0 + (int)(((float)(y1 - y0) / (float)(y2 - y0)) * (x2 - x0));
    int y3 = y1;

    gfx_fill_flat_bottom_triangle(surface, x0, y0, x1, y1, x3, y3, color);
    gfx_fill_flat_top_triangle(surface, x1, y1, x3, y3, x2, y2, color);
  }
}

int gfx_get_string_width(const char *str) {
  if (!str)
    return 0;
  int len = 0;
  while (*str++)
    len++;
  return len * 8;
}

/* ==========================================================================
 * Panels and 3D borders
 * ======================================================================= */
void gfx_panel(gfx_surface_t *surface, int x, int y, int w, int h,
               uint32_t bg) {
  gfx_fill_rect(surface, x, y, w, h, bg);
}

void gfx_bevel_out(gfx_surface_t *surface, int x, int y, int w, int h) {
  uint32_t light = gfx_theme_color(GFX_BORDER_LIGHT);
  uint32_t dark = gfx_theme_color(GFX_BORDER_DARK);
  gfx_hline(surface, x, y, w, light);
  gfx_vline(surface, x, y, h, light);
  gfx_hline(surface, x, y + h - 1, w, dark);
  gfx_vline(surface, x + w - 1, y, h, dark);
}

void gfx_bevel_in(gfx_surface_t *surface, int x, int y, int w, int h) {
  uint32_t light = gfx_theme_color(GFX_BORDER_LIGHT);
  uint32_t dark = gfx_theme_color(GFX_BORDER_DARK);
  gfx_hline(surface, x, y, w, dark);
  gfx_vline(surface, x, y, h, dark);
  gfx_hline(surface, x, y + h - 1, w, light);
  gfx_vline(surface, x + w - 1, y, h, light);
}

/* ==========================================================================
 * Title bar
 * ======================================================================= */
void gfx_title_bar(gfx_surface_t *surface, int x, int y, int w,
                   const char *title) {
  uint32_t bg = gfx_theme_color(GFX_BG_TITLE);
  uint32_t fg = gfx_theme_color(GFX_FG_TEXT);

  gfx_fill_rect(surface, x, y, w, 20, bg);
  if (title) {
    gfx_draw_string(surface, x + 4, y + 6, title, fg);
  }
  gfx_bevel_out(surface, x, y, w, 20);
}

/* ==========================================================================
 * Button
 * ======================================================================= */
void gfx_button(gfx_surface_t *surface, int x, int y, int w, int h,
                const char *label, int pressed) {
  uint32_t bg = pressed ? gfx_theme_color(GFX_BG_BUTTON_HOVER)
                        : gfx_theme_color(GFX_BG_BUTTON);
  uint32_t fg = gfx_theme_color(GFX_FG_TEXT);

  gfx_fill_rect(surface, x, y, w, h, bg);
  if (pressed == 1) {
    gfx_bevel_in(surface, x, y, w, h);
  } else {
    gfx_bevel_out(surface, x, y, w, h);
  }

  if (label) {
    int tw = gfx_get_string_width(label);
    int tx = x + (w - tw) / 2;
    int ty = y + (h - 8) / 2;
    gfx_draw_string(surface, tx, ty, label, fg);
  }
}

int ui_button(gfx_surface_t *surface, int x, int y, int w, int h,
              const char *label) {
  int hover = point_in_rect(mouse_x, mouse_y, x, y, w, h);

  int pressed = hover && mouse_left_down();

  gfx_button(surface, x, y, w, h, label, pressed);

  return hover && mouse_left_pressed();
}

/* ==========================================================================
 * Progress bar
 * ======================================================================= */
void gfx_progress_bar(gfx_surface_t *surface, int x, int y, int w, int h,
                      int percent, uint32_t fill, uint32_t empty) {
  gfx_fill_rect(surface, x, y, w, h, empty);
  gfx_bevel_in(surface, x, y, w, h);

  int fill_w = (w - 4) * percent / 100;
  if (fill_w > 0) {
    gfx_fill_rect(surface, x + 2, y + 2, fill_w, h - 4, fill);
  }
}

/* ==========================================================================
 * List box
 * ======================================================================= */
void gfx_list(gfx_surface_t *surface, int x, int y, int w, int h,
              const char **items, int count, int selected) {
  uint32_t bg = gfx_theme_color(GFX_BG_PANEL);
  uint32_t fg = gfx_theme_color(GFX_FG_TEXT);
  uint32_t hi = gfx_theme_color(GFX_BG_HIGHLIGHT);
  uint32_t hifg = gfx_theme_color(GFX_FG_ACCENT);

  gfx_fill_rect(surface, x, y, w, h, bg);
  gfx_bevel_in(surface, x, y, w, h);

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

  for (int i = 0; i < visible && (start + i) < count; i++) {
    int idx = start + i;
    int row_y = content_y + i * row_h;
    uint32_t row_bg = (idx == selected) ? hi : bg;
    uint32_t row_fg = (idx == selected) ? hifg : fg;

    gfx_fill_rect(surface, content_x, row_y, content_w, row_h, row_bg);
    gfx_draw_string(surface, content_x + 4, row_y + 6, items[idx], row_fg);
  }
}

/* ==========================================================================
 * Status / task bar
 * ======================================================================= */
void gfx_status_bar(gfx_surface_t *surface, int x, int y, int w,
                    const char *text) {
  uint32_t bg = gfx_theme_color(GFX_BG_TITLE);
  uint32_t fg = gfx_theme_color(GFX_FG_TEXT_DIM);

  gfx_fill_rect(surface, x, y, w, 24, bg);
  gfx_bevel_out(surface, x, y, w, 24);
  if (text) {
    gfx_draw_string(surface, x + 4, y + 8, text, fg);
  }
}

/* ==========================================================================
 * Desktop background
 * ======================================================================= */
void gfx_desktop(gfx_surface_t *surface) {
  gfx_clear(surface, gfx_theme_color(GFX_BG_DESKTOP));
}

/* ==========================================================================
 * Design 2
 * ======================================================================= */
void gfx_logo_design2(gfx_surface_t *surface, int x, int y) {
  gfx_fill_rect(surface, x, y, 150, 150, gfx_theme_color(GFX_RED));
  gfx_fill_rect(surface, x + 25, y + 25, 150, 150, gfx_theme_color(GFX_GREEN));
  gfx_fill_rect(surface, x + 50, y + 50, 150, 150, gfx_theme_color(GFX_BLUE));
  gfx_fill_rect(surface, x + 50, y + 50, 125, 125, gfx_theme_color(GFX_RED));
  gfx_draw_string(surface, x, y - 10, "Welcome to AmitX!",
                  gfx_theme_color(GFX_WHITE));
}

void gfx_logo_os(gfx_surface_t *surface, int x, int y) {
  gfx_logo_phonon(surface, x, y);
  gfx_logo_shadow(surface, x + 160, y);
}

void gfx_logo_phonon(gfx_surface_t *surface, int x, int y) {
  gfx_fill_rect(surface, x + 5, y + 5, 70, 70, fb_pack_pixel(0xF7, 0xA8, 0xB8));
  gfx_fill_rect(surface, x + 80, y, 70, 70, gfx_theme_color(GFX_GREEN));
  gfx_fill_rect(surface, x, y + 80, 70, 70, gfx_theme_color(GFX_BLUE));
  gfx_fill_rect(surface, x + 75, y + 75, 70, 70, gfx_theme_color(GFX_RED));
  gfx_draw_string(surface, x + 10, y - 10, "Welcome to Phonon!",
                  gfx_theme_color(GFX_WHITE));
}

void gfx_logo_shadow(gfx_surface_t *surface, int x, int y) {
  gfx_draw_rect(surface, x, y, 70, 70, fb_pack_pixel(0xF7, 0xA8, 0xB8));
  gfx_draw_rect(surface, x + 75, y + 5, 70, 70, gfx_theme_color(GFX_GREEN));
  gfx_draw_rect(surface, x + 5, y + 75, 70, 70, gfx_theme_color(GFX_BLUE));
  gfx_fill_rect(surface, x + 80, y + 80, 70, 70, gfx_theme_color(GFX_RED));
  gfx_draw_string(surface, x, y - 10, "Powered by Shadow!",
                  gfx_theme_color(GFX_WHITE));
}

int point_in_rect(int px, int py, int x, int y, int w, int h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}