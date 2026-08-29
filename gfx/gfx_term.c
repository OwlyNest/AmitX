/*
 * gfx/gfx_term.c - [Enter description]
 * Author:   amity
 * Date:     Fri Jun 26 09:42:09 2026
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
#include <gfx/fb.h>
#include <gfx/font.h>
#include <gfx/gfx_term.h>
#include <lib/string.h>
/* --- Includes ---*/
static int term_px, term_py;      /* panel position */
static int term_pw, term_ph;      /* panel size */
static int term_tx, term_ty;      /* text area position */
static int term_tw, term_th;      /* text area size */
static int term_cols, term_rows;  /* dimensions in chars */
static int term_cx, term_cy;      /* cursor in chars */
static uint32_t term_fg, term_bg; /* colors */

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
static void term_scroll_if_needed(void);

/* --- Functions ---*/
/* ==========================================================================
 * Initialize terminal
 * ======================================================================= */
void gfx_term_init(int x, int y, int w, int h, uint32_t fg, uint32_t bg) {
  term_px = x;
  term_py = y;
  term_pw = w;
  term_ph = h;

  /* Text area inside panel with margins */
  term_tx = x + 8;
  term_ty = y + 28;
  term_tw = w - 16;
  term_th = h - 36;

  term_cols = term_tw / 8;
  term_rows = term_th / 8;
  term_cx = 0;
  term_cy = 0;
  term_fg = fg;
  term_bg = bg;
}

/* ==========================================================================
 * Draw frame: panel, border, title bar
 * ======================================================================= */
void gfx_term_draw_frame(const char *title) {
  gfx_desktop(&fb.back);

  /* Panel background */
  fb_fill_rect(term_px, term_py, term_pw, term_ph,
               gfx_theme_color(GFX_BG_PANEL));
  gfx_bevel_in(&fb.back, term_px, term_py, term_pw, term_ph);

  /* Title bar */
  gfx_title_bar(&fb.back, term_px, term_py, term_pw, title);

  /* Clear text area */
  fb_fill_rect(term_tx, term_ty, term_tw, term_th, term_bg);

  term_cx = 0;
  term_cy = 0;
}

/* ==========================================================================
 * Clear text area only
 * ======================================================================= */
void gfx_term_clear(void) {
  fb_fill_rect(term_tx, term_ty, term_tw, term_th, term_bg);
  term_cx = 0;
  term_cy = 0;
}

/* ==========================================================================
 * Scroll: shift text up, clear bottom line
 * ======================================================================= */
static void term_scroll_if_needed(void) {
  if (term_cy < term_rows)
    return;

  /* Simple wrap: clear and restart from top */
  gfx_term_clear();
}

/* ==========================================================================
 * Put single character
 * ======================================================================= */
void gfx_term_putc(char c) {
  if (c == '\n') {
    gfx_term_newline();
    return;
  }
  if (c == '\b') {
    gfx_term_backspace();
    return;
  }
  if (c < 32 || c > 126)
    c = '?';

  if (term_cx >= term_cols) {
    gfx_term_newline();
  }

  fb_draw_char((uint32_t)(term_tx + term_cx * 8),
               (uint32_t)(term_ty + term_cy * 8), c, term_fg);
  term_cx++;
}

/* ==========================================================================
 * Put string
 * ======================================================================= */
void gfx_term_puts(const char *str) {
  if (!str)
    return;
  while (*str) {
    gfx_term_putc(*str++);
  }
}

/* ==========================================================================
 * Newline
 * ======================================================================= */
void gfx_term_newline(void) {
  term_cx = 0;
  term_cy++;
  term_scroll_if_needed();
}

/* ==========================================================================
 * Backspace
 * ======================================================================= */
void gfx_term_backspace(void) {
  if (term_cx > 0) {
    term_cx--;
    /* Erase char at cursor */
    fb_fill_rect((uint32_t)(term_tx + term_cx * 8),
                 (uint32_t)(term_ty + term_cy * 8), 8, 8, term_bg);
  }
}

/* ==========================================================================
 * Put integer
 * ======================================================================= */
void gfx_term_putint(int num) {
  char buf[16];
  itoa(num, buf);
  gfx_term_puts(buf);
}

/* ==========================================================================
 * Put hex
 * ======================================================================= */
void gfx_term_puthex(uint32_t n) {
  gfx_term_puts("0x");
  char hex_chars[] = "0123456789ABCDEF";
  int started = 0;
  for (int i = 7; i >= 0; i--) {
    uint8_t nibble = (n >> (i * 4)) & 0xF;
    if (nibble != 0 || started || i == 0) {
      gfx_term_putc(hex_chars[nibble]);
      started = 1;
    }
  }
}

/* ==========================================================================
 * Move cursor (in character cells, 0-based)
 * ======================================================================= */
void gfx_term_move_cursor(int col, int row) {
  term_cx = col;
  term_cy = row;
  if (term_cx < 0)
    term_cx = 0;
  if (term_cx >= term_cols)
    term_cx = term_cols - 1;
  if (term_cy < 0)
    term_cy = 0;
  if (term_cy >= term_rows)
    term_cy = term_rows - 1;
}

/* ==========================================================================
 * Draw prompt at current cursor position
 * ======================================================================= */
void gfx_term_draw_prompt(void) { gfx_term_puts("[::]> "); }