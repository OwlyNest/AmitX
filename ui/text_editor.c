/*
	* ui/text_editor.c - Text editor (windowed)
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
#define ED_W        800
#define ED_H        600
#define ED_MARGIN_X 8
#define ED_MARGIN_Y 28
#define ED_STATUS_H 24

/* --- Includes ---*/
#include <ui/text_editor.h>
#include <fs/amfs.h>
#include <gfx/window.h>
#include <gfx/gfx_term.h>
#include <gfx/fb.h>
#include <gfx/font.h>
#include <drivers/keyboard.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <mm/heap.h>

/* --- Typedefs - Structs - Enums ---*/
window_handle_t editor;
static window_t *win;

/* --- Globals ---*/
static editor_t ed;

/* --- Prototypes ---*/
static void ed_init(const char *path);
static void ed_free(void);
static void ed_grow(size_t min_size);
static void ed_load(const char *path);
static void ed_save(void);
static void ed_insert(char c);
static void ed_delete(void);
static size_t ed_line_start(size_t pos);
static size_t ed_line_end(size_t pos);
static size_t ed_prev_line_start(size_t pos);
static size_t ed_next_line_start(size_t pos);
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

/* --- Functions ---*/

/* --- Buffer management --- */
static void ed_init(const char *path) {
    memset(&ed, 0, sizeof(ed));
    strncpy(ed.path, path, 127);
    ed.path[127] = '\0';

    /* Text area inside window with margins */
    int tw = ED_W - ED_MARGIN_X * 2;
    int th = ED_H - ED_MARGIN_Y - ED_STATUS_H - 8;

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
    memmove(ed.data + ed.cursor - 1, ed.data + ed.cursor,
            ed.size - ed.cursor + 1);
    ed.size--;
    ed.cursor--;
    ed.dirty = 1;
    ed_update_cx_cy();
    ed_ensure_visible();
}

/* --- Drawing --- */

static void ed_status(const char *msg) {
    /* Drawn at bottom of window */
    uint32_t bg = gfx_theme_color(GFX_BG_TITLE);
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT_DIM);
    int y = ED_H - ED_STATUS_H;

    gfx_fill_rect(&win->surface, 0, y, ED_W, ED_STATUS_H, bg);
    gfx_bevel_out(&win->surface, 0, y, ED_W, ED_STATUS_H);

    char buf[128];
    const char *name = ed.path[0] ? ed.path : "[untitled]";
    const char *mod = ed.dirty ? " [modified]" : "";
    ksnprintf(buf, sizeof(buf), " %s%s  |  %s", name, mod, msg ? msg : "");
    window_draw_text(editor, 4, y + 6, buf, fg);
}

static void ed_draw(void) {
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);
    uint32_t hi = gfx_theme_color(GFX_BG_HIGHLIGHT);

    /* Clear text area */
    gfx_fill_rect(&win->surface, ED_MARGIN_X, ED_MARGIN_Y, ED_W - ED_MARGIN_X * 2, ED_H - ED_MARGIN_Y - ED_STATUS_H - 8, bg);

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

            int draw_y = ED_MARGIN_Y + (screen_y * 8);

            /* Draw line content — use window surface, not fb.back */
            for (int i = 0; i < len; i++) {
                char c = ed.data[pos + i];
                if (c < 32 || c > 126) c = '?';
                gfx_draw_char(&win->surface, ED_MARGIN_X + i * 8, draw_y, c, fg);
            }

            /* Draw cursor if on this line */
            if (ed.cy == (int)line) {
                int cx = ed.cx;
                if (cx > ed.cols - 1) cx = ed.cols - 1;
                int cx_px = ED_MARGIN_X + cx * 8;
                int cy_px = draw_y;
                /* Invert cursor: draw highlight rect */
                gfx_fill_rect(&win->surface, cx_px, cy_px, 8, 8, hi);
                if (ed.cursor < ed.size &&
                    ed.data[ed.cursor] != '\n') {
                    gfx_draw_char(&win->surface, cx_px, cy_px, ed.data[ed.cursor], bg);
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
}

/* --- Main loop --- */

void amity_run(const char *path) {
    int win_x = (fb.back.width - ED_W) / 2;
    int win_y = (fb.back.height - ED_H) / 2;

    editor = window_create(win_x, win_y, ED_W, ED_H, "amity", WIN_FLAG_TITLEBAR);
    if ((int)editor == WIN_INVALID) {
        printk("[amity] Failed to create window\n");
        return;
    }

    win = window_get(editor);
    if (!win) {
        printk("[amity] Failed to get window\n");
        return;
    }

    if (!path || path[0] != '/') {
        /* Can't edit without a path */
        window_destroy(editor);
        return;
    }

    ed_init(path);
    ed_load(path);

    /* Draw initial frame */
    gfx_desktop(&win->surface);

    /* Title bar at top of window */
    gfx_fill_rect(&win->surface, 0, 0, ED_W, 20, gfx_theme_color(GFX_BG_TITLE));
    gfx_bevel_out(&win->surface, 0, 0, ED_W, 20);
    char title[128];
    ksnprintf(title, sizeof(title), " amity — %s ", path);
    window_draw_text(editor, 4, 6, title, gfx_theme_color(GFX_FG_TEXT));

    ed_draw();
    window_present_all();

    while (1) {
        unsigned char c = keyboard_getchar();

        if (c == KEY_UP) {
            ed_cursor_up();
            ed_draw();
            window_present_all();
        } else if (c == KEY_DOWN) {
            ed_cursor_down();
            ed_draw();
            window_present_all();
        } else if (c == KEY_LEFT) {
            ed_cursor_left();
            ed_draw();
            window_present_all();
        } else if (c == KEY_RIGHT) {
            ed_cursor_right();
            ed_draw();
            window_present_all();
        } else if (c == '\b') {
            ed_delete();
            ed_draw();
            window_present_all();
        } else if (c == '\n') {
            ed_insert('\n');
            ed_draw();
            window_present_all();
        } else if (c == 0x13) {  /* Ctrl+S */
            ed_save();
            ed_draw();
            window_present_all();
        } else if (c == 0x11) {  /* Ctrl+Q */
            if (ed.dirty) {
                ed_status("Unsaved changes! Ctrl+Q again to quit");
                ed_draw();
                window_present_all();
                /* Wait for second Ctrl+Q */
                unsigned char confirm = keyboard_getchar();
                if (confirm == 0x11) {
                    break;
                } else {
                    ed_status(NULL);
                    ed_draw();
                    window_present_all();
                }
            } else {
                break;
            }
        } else if (c == 0x01) {  /* Ctrl+A — home */
            ed_cursor_home();
            ed_draw();
            window_present_all();
        } else if (c == 0x05) {  /* Ctrl+E — end */
            ed_cursor_end();
            ed_draw();
            window_present_all();
        } else if (c >= 32 && c < 128) {
            ed_insert((char)c);
            ed_draw();
            window_present_all();
        }
    }

    ed_free();
    window_destroy(editor);
}