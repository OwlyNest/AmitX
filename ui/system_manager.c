/*
	* ui/system_manager.c - [Enter description]
	* Author:   amity
	* Date:     Tue Jun 16 23:33:37 2026
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

//* --- Macros ---*/
#define SM_X        112
#define SM_Y        50
#define SM_W        800
#define SM_H        668
#define SM_ROW_H    20

/* --- Includes ---*/
#include <ui/system_manager.h>
#include <internal/kscope.h>
#include <gfx/gfx_screen.h>
#include <gfx/fb.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <screen/printk.h>
#include <lib/string.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static int sm_selected = 0;

/* --- Prototypes ---*/
static void sm_draw_list(void);
static void sm_draw_detail(const kscope_node_t *node);
static void sm_present(void);

/* --- Functions ---*/

/* ==========================================================================
 * Present helper: hide cursor, draw, show cursor, present
 * ======================================================================= */
static void sm_present(void) {
    fb_present();
}

/* ==========================================================================
 * State string
 * ======================================================================= */
static const char* sm_state_str(kscope_state_t state) {
    switch (state) {
        case KSCOPE_STATE_FAILED:      return "FAIL";
        case KSCOPE_STATE_REGISTERED:  return "REG";
        case KSCOPE_STATE_PROBING:     return "PROB";
        case KSCOPE_STATE_OK:          return "OK";
        default:                       return "ERR";
    }
}

/* ==========================================================================
 * Draw list view
 * ======================================================================= */
static void sm_draw_list(void) {
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);
    uint32_t hi = gfx_theme_color(GFX_BG_HIGHLIGHT);
    uint32_t hifg = gfx_theme_color(GFX_FG_ACCENT);

    gfx_desktop();

    gfx_panel(SM_X, SM_Y, SM_W, SM_H, bg);
    gfx_bevel_in(SM_X, SM_Y, SM_W, SM_H);
    gfx_title_bar(SM_X, SM_Y, SM_W, " System Manager ");

    int list_x = SM_X + 8;
    int list_y = SM_Y + 28;
    int list_w = SM_W - 16;
    int list_h = SM_H - 60;

    int visible = list_h / SM_ROW_H;
    int start = 0;
    if (sm_selected >= visible) {
        start = sm_selected - visible + 1;
    }

    gfx_set_clip(list_x, list_y, list_w, list_h);

    for (int i = 0; i < visible && (start + i) < (int)kscope_get_count(); i++) {
        int idx = start + i;
        int row_y = list_y + i * SM_ROW_H;
        uint32_t row_bg = (idx == sm_selected) ? hi : bg;
        uint32_t row_fg = (idx == sm_selected) ? hifg : fg;

        gfx_fill_rect1(list_x, row_y, list_w, SM_ROW_H, row_bg);

        const kscope_node_t *node = kscope_get_node(idx);
        char line[80];
        ksnprintf(line, sizeof(line),
                  "[0x%04x] (%s) %-16s %-12s",
                  node->id, sm_state_str(node->state),
                  node->name, kscope_class_name(node->class));

        gfx_draw_text(list_x + 4, row_y + 6, line, row_fg);
    }

    gfx_clear_clip();

    gfx_status_bar(0, 744, 1024,
        " [Up/Down] Select  [Enter] Details  [q] Back ");

    sm_present();
}

/* ==========================================================================
 * Draw detail view
 * ======================================================================= */
static void sm_draw_detail(const kscope_node_t *node) {
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);

    gfx_desktop();

    gfx_panel(SM_X, SM_Y, SM_W, SM_H, bg);
    gfx_bevel_in(SM_X, SM_Y, SM_W, SM_H);

    char title[64];
    ksnprintf(title, sizeof(title),
              " [0x%04x] %s ", node->id, node->name);
    gfx_title_bar(SM_X, SM_Y, SM_W, title);

    int x = SM_X + 16;
    int y = SM_Y + 36;
    char buf[80];

    ksnprintf(buf, sizeof(buf), "  Class:     %s",
              kscope_class_name(node->class));
    gfx_draw_text(x, y, buf, fg); y += 16;

    ksnprintf(buf, sizeof(buf), "  Subclass:  %s",
              kscope_subclass_name(node->subclass));
    gfx_draw_text(x, y, buf, fg); y += 16;

    ksnprintf(buf, sizeof(buf), "  State:     %s",
              sm_state_str(node->state));
    gfx_draw_text(x, y, buf, fg); y += 24;

    gfx_draw_text(x, y, "Dependencies:", fg); y += 16;
    if (node->require_count != 0) {
        for (size_t i = 0; i < node->require_count; i++) {
            ksnprintf(buf, sizeof(buf), "    - %s [%s]",
                      node->requires[i]->name,
                      sm_state_str(node->requires[i]->state));
            gfx_draw_text(x, y, buf, fg); y += 16;
        }
    } else {
        gfx_draw_text(x, y, "    - none", fg); y += 16;
    }

    y += 8;
    gfx_draw_text(x, y, "Capabilities:", fg); y += 16;
    if (node->provide_count != 0) {
        for (size_t i = 0; i < node->provide_count; i++) {
            ksnprintf(buf, sizeof(buf), "    - %s", node->provides[i]);
            gfx_draw_text(x, y, buf, fg); y += 16;
        }
    } else {
        gfx_draw_text(x, y,
            "    - Why in the name of Owly does this exist?", fg);
        y += 16;
    }

    gfx_status_bar(0, 744, 1024, " [q] Back ");

    sm_present();
}

/* ==========================================================================
 * Main loop
 * ======================================================================= */
void system_manager_run(void) {
    sm_selected = 0;
    sm_draw_list();

    while (1) {
        unsigned char c = keyboard_getchar();

        if (c == 's' || c == KEY_DOWN) {
            if (sm_selected < (int)kscope_get_count() - 1) {
                sm_selected++;
                sm_draw_list();
            }
        } else if (c == 'w' || c == KEY_UP) {
            if (sm_selected > 0) {
                sm_selected--;
                sm_draw_list();
            }
        } else if (c == '\n') {
            if (kscope_get_count() > 0) {
                const kscope_node_t *node =
                    kscope_get_node(sm_selected);
                sm_draw_detail(node);

                while (1) {
                    unsigned char d = keyboard_getchar();
                    if (d == 'q' || d == KEY_ESC) {
                        sm_draw_list();
                        break;
                    }
                }
            }
        } else if (c == 'q' || c == KEY_ESC) {
            break;
        }
    }
}