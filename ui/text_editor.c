/*
	* ui/text_editor.c - [Enter description]
	* Author:   amity
	* Date:     Sun Jun 28 12:32:58 2026
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
#include <ui/text_editor.h>
#include <fs/amfs.h>
#include <gfx/gfx_screen.h>
#include <gfx/gfx_term.h>
#include <gfx/fb.h>
#include <gfx/font.h>
#include <drivers/keyboard.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <mm/heap.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static editor_t ed;
/* --- Prototypes ---*/
static void ed_init(const char *path);
static void ed_free(void);
static void ed_load(const char *path);
static void ed_save(void);
static void ed_insert(char c);
static void ed_delete(void);
static void ed_cursor_up(void);
static void ed_cursor_down(void);
static void ed_cursor_left(void);
static void ed_cursor_right(void);
static void ed_cursor_home(void);
static void ed_cursor_end(void);
static void ed_update_cx_cy(void);
static void ed_ensure_visible(void);
static void ed_draw(void);
static void ed_status(const char *msg);
static void amity_present(void);
/* --- Functions ---*/

/* --- Buffer management --- */
static void ed_init(const char *path) {
	memset(&ed, 0, sizeof(ed));
	strncpy(ed.path, path, 127);
	ed.path[127] = '\0';

	/* Text area inside panel with margins */
	// int tx = EDITOR_MARGIN_X;
	// int ty = EDITOR_MARGIN_Y;
	int tw = (int)fb.width - 16;
    int th = (int)fb.height - EDITOR_MARGIN_Y - EDITOR_STATUS_H - 8;

	ed.cols = tw / 8;
	ed.rows = (th - 4) / 8;

	/* Start with 1KB buffer, grow as needed */
    ed.capacity = 1024;
    ed.data = (char *)malloc(ed.capacity);
    if (!ed.data) {
        ed.capacity = 0;
        return;
    }
    ed.data[0] = '\0';
    ed.size = 0;
    ed.cursor = 0;
    ed.scroll_y = 0;
    ed.cx = 0;
    ed.cy = 0;
    ed.dirty = 0;
}

static void ed_free(void) {
    if (ed.data) {
        free(ed.data);
        ed.data = NULL;
    }
}

static void ed_grow(size_t min_size) {
	if (min_size <= ed.capacity) return;
    size_t new_cap = ed.capacity * 2;
    while (new_cap < min_size) new_cap *= 2;
    char *new_data = (char *)malloc(new_cap);
    if (!new_data) return;
    memcpy(new_data, ed.data, ed.size);
    free(ed.data);
    ed.data = new_data;
    ed.capacity = new_cap;
}

/* --- File I/O --- */
static void ed_load(const char *path) {
	if (!amfs_exists(path)) {
		/* New file — empty buffer, cursor at 0 */
        ed.dirty = 0;
        return;
	}

	char buf[4096];
	int len = amfs_read(path, buf, sizeof(buf) - 1);
	if (len < 0) {
        ed_status("Failed to read file");
        return;
    }
    buf[len] = '\0';

	ed_grow(len + 1);
    memcpy(ed.data, buf, len + 1);
    ed.size = len;
    ed.cursor = 0;
    ed.dirty = 0;
    ed_update_cx_cy();
}

static void ed_save(void) {
    if (!ed.dirty) {
        ed_status("No changes to save");
        return;
    }

    if (amfs_write(ed.path, ed.data, ed.size) != 0) {
        ed_status("Failed to save file");
        return;
    }

    ed.dirty = 0;
    ed_status("Saved");
}

/* --- Cursor movement --- */

static size_t ed_line_start(size_t pos) {
    while (pos > 0 && ed.data[pos - 1] != '\n') pos--;
    return pos;
}

static size_t ed_line_end(size_t pos) {
    while (pos < ed.size && ed.data[pos] != '\n') pos++;
    return pos;
}

static size_t ed_prev_line_start(size_t pos) {
    size_t start = ed_line_start(pos);
    if (start == 0) return 0;
    return ed_line_start(start - 1);
}

static size_t ed_next_line_start(size_t pos) {
    size_t end = ed_line_end(pos);
    if (end >= ed.size) return ed.size;
    return end + 1;
}

static void ed_update_cx_cy(void) {
    /* Count lines and columns to cursor */
    size_t line_start = ed_line_start(ed.cursor);
    ed.cx = ed.cursor - line_start;

    size_t pos = 0;
    ed.cy = 0;
    while (pos < line_start) {
        if (ed.data[pos] == '\n') ed.cy++;
        pos++;
    }
}

static void ed_ensure_visible(void) {
    if (ed.cy < (int)ed.scroll_y) {
        ed.scroll_y = ed.cy;
    } else if (ed.cy >= (int)ed.scroll_y + ed.rows) {
        ed.scroll_y = ed.cy - ed.rows + 1;
    }
}

static void ed_cursor_up(void) {
    size_t prev = ed_prev_line_start(ed.cursor);
    if (prev == ed.cursor) return; /* Already on first line */

    size_t prev_end = ed_line_end(prev);
    size_t offset = ed.cursor - ed_line_start(ed.cursor);
    if (offset > prev_end - prev) offset = prev_end - prev;
    ed.cursor = prev + offset;
    ed_update_cx_cy();
    ed_ensure_visible();
}

static void ed_cursor_down(void) {
    size_t next = ed_next_line_start(ed.cursor);
    if (next == ed.size && ed.cursor == ed.size) return;

    size_t next_end = ed_line_end(next);
    size_t offset = ed.cursor - ed_line_start(ed.cursor);
    if (offset > next_end - next) offset = next_end - next;
    ed.cursor = next + offset;
    if (ed.cursor > ed.size) ed.cursor = ed.size;
    ed_update_cx_cy();
    ed_ensure_visible();
}

static void ed_cursor_left(void) {
    if (ed.cursor > 0) {
        ed.cursor--;
        ed_update_cx_cy();
        ed_ensure_visible();
    }
}

static void ed_cursor_right(void) {
    if (ed.cursor < ed.size) {
        ed.cursor++;
        ed_update_cx_cy();
        ed_ensure_visible();
    }
}

static void ed_cursor_home(void) {
    ed.cursor = ed_line_start(ed.cursor);
    ed_update_cx_cy();
    ed_ensure_visible();
}

static void ed_cursor_end(void) {
    ed.cursor = ed_line_end(ed.cursor);
    ed_update_cx_cy();
    ed_ensure_visible();
}

/* --- Insert and delete --- */

static void ed_insert(char c) {
    ed_grow(ed.size + 2);
    memmove(ed.data + ed.cursor + 1, ed.data + ed.cursor, ed.size - ed.cursor + 1);
    ed.data[ed.cursor] = c;
    ed.size++;
    ed.cursor++;
    ed.dirty = 1;
    ed_update_cx_cy();
    ed_ensure_visible();
}

static void ed_delete(void) {
    if (ed.cursor == 0) return;
    memmove(ed.data + ed.cursor - 1, ed.data + ed.cursor, ed.size - ed.cursor + 1);
    ed.size--;
    ed.cursor--;
    ed.dirty = 1;
    ed_update_cx_cy();
    ed_ensure_visible();
}

/* --- Drawing --- */

static void amity_present(void) {
    fb_present();
}

static void ed_status(const char *msg) {
    /* Drawn at bottom of screen */
    uint32_t bg = gfx_theme_color(GFX_BG_TITLE);
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT_DIM);
    int y = (int)fb.height - EDITOR_STATUS_H;

    gfx_fill_rect(0, y, (int)fb.width, EDITOR_STATUS_H, bg);
    gfx_bevel_out(0, y, (int)fb.width, EDITOR_STATUS_H);

    char buf[128];
    const char *name = ed.path[0] ? ed.path : "[untitled]";
    const char *mod = ed.dirty ? " [modified]" : "";
    ksnprintf(buf, sizeof(buf), " %s%s  |  %s", name, mod, msg ? msg : "");
    gfx_draw_text(4, y + 6, buf, fg);
}

static void ed_draw(void) {
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);
    uint32_t hi = gfx_theme_color(GFX_BG_HIGHLIGHT);

    /* Clear text area */
    gfx_fill_rect(EDITOR_MARGIN_X, EDITOR_MARGIN_Y,
                  (int)fb.width - 16,
                  (int)fb.height - EDITOR_MARGIN_Y - EDITOR_STATUS_H - 8,
                  bg);

    /* Draw text */
    uint32_t line = 0;
    size_t pos = 0;
    int screen_y = 0;

    while (pos <= ed.size && screen_y < ed.rows) {
        if (line >= ed.scroll_y) {
            /* Find end of this line */
            uint32_t line_end = pos;
            while (line_end < ed.size && ed.data[line_end] != '\n')
                line_end++;

            int len = line_end - pos;
            if (len > ed.cols) len = ed.cols;

            int draw_y = EDITOR_MARGIN_Y + (screen_y * 8);

            /* Draw line content */
            for (int i = 0; i < len; i++) {
                char c = ed.data[pos + i];
                if (c < 32 || c > 126) c = '?';
                fb_draw_char((uint32_t)(EDITOR_MARGIN_X + i * 8), (uint32_t)draw_y, c, fg);
            }

            /* Draw cursor if on this line */
            if (ed.cy == (int)line) {
                int cx = ed.cx;
                if (cx > ed.cols - 1) cx = ed.cols - 1;
                uint32_t cx_px = (uint32_t)(EDITOR_MARGIN_X + cx * 8);
                uint32_t cy_px = (uint32_t)draw_y;
                /* Invert cursor: draw highlight rect */
                gfx_fill_rect((int)cx_px, (int)cy_px, 8, 8, hi);
                if (ed.cursor < ed.size && ed.data[ed.cursor] != '\n') {
                    fb_draw_char(cx_px, cy_px, ed.data[ed.cursor], bg);
                }
            }

            screen_y++;
        }

        /* Advance to next line */
        if (pos < ed.size && ed.data[pos] == '\n') {
            pos++;
        } else {
            while (pos < ed.size && ed.data[pos] != '\n') pos++;
            if (pos < ed.size && ed.data[pos] == '\n') pos++;
        }
        line++;
    }

    ed_status(NULL);
    amity_present();
}

/* --- Main loop --- */

void amity_run(const char *path) {
    if (!path || path[0] != '/') {
        /* Can't edit without a path */
        return;
    }

    ed_init(path);
    ed_load(path);

    /* Draw initial frame */
    gfx_desktop();
    gfx_fill_rect(0, 0, (int)fb.width, 20, gfx_theme_color(GFX_BG_TITLE));
    gfx_bevel_out(0, 0, (int)fb.width, 20);
    char title[128];
    ksnprintf(title, sizeof(title), " amity — %s ", path);
    gfx_draw_text(4, 6, title, gfx_theme_color(GFX_FG_TEXT));

    ed_draw();

    while (1) {
        unsigned char c = keyboard_getchar();

        if (c == KEY_UP) {
            ed_cursor_up();
            ed_draw();
        } else if (c == KEY_DOWN) {
            ed_cursor_down();
            ed_draw();
        } else if (c == KEY_LEFT) {
            ed_cursor_left();
            ed_draw();
        } else if (c == KEY_RIGHT) {
            ed_cursor_right();
            ed_draw();
        } else if (c == '\b') {
            ed_delete();
            ed_draw();
        } else if (c == '\n') {
            ed_insert('\n');
            ed_draw();
            /* Check for Ctrl+S (0x13) and Ctrl+Q (0x11) */
		}else if (c == 0x13) {  /* Ctrl+S */
			ed_save();
			ed_draw();
		} else if (c == 0x11) {  /* Ctrl+Q */
			if (ed.dirty) {
				ed_status("Unsaved changes! Ctrl+Q again to quit");
				ed_draw();
				/* Wait for second Ctrl+Q */
				unsigned char confirm = keyboard_getchar();
				if (confirm == 0x11) {
					break;
				} else {
					ed_status(NULL);
					ed_draw();
					/* Put the char back if it's printable? Nah, simpler to ignore */
				}
			} else {
				break;
			}
		} else if (c == 0x01) {  /* Ctrl+A — home */
			ed_cursor_home();
			ed_draw();
		} else if (c == 0x05) {  /* Ctrl+E — end */
			ed_cursor_end();
			ed_draw();
		} else if (c >= 32 && c < 128) {
			ed_insert((char)c);
			ed_draw();
		}
    }

    ed_free();
}