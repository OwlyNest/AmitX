/*
	* ui/about.c - About dialog (windowed)
	* Author:   amity
	* Date:     Wed Jul  1 14:20:28 2026
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
#define ABOUT_W        400
#define ABOUT_H        350

/* --- Includes ---*/
#include <ui/about.h>
#include <gfx/window.h>
#include <gfx/fb.h>
#include <gfx/compositor.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <hw/pci.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <internal/amitx_info.h>
#include <mm/pmm.h>
#include <logo/logo.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
window_handle_t about;
static window_t *win;

/* --- Prototypes ---*/
static void about_draw(void);
static void about_draw_authors(void);

/* --- Functions ---*/

/* ==========================================================================
 * Draw main about view
 * ======================================================================= */
static void about_draw(void) {
    gfx_desktop(&win->surface);

    /* Beveled frame inside window */
	gfx_panel(&win->surface, 8, 8, ABOUT_W - 16, ABOUT_H - 16, gfx_theme_color(GFX_BG_PANEL));
    gfx_bevel_in(&win->surface, 8, 8, ABOUT_W - 16, ABOUT_H - 16);
    gfx_title_bar(&win->surface, 8, 8, ABOUT_W - 16, " About ");

    /* Logo centered in upper area */
    int logo_x = (ABOUT_W - 85);
    int logo_y = 25;
    draw_logo_gfx(&win->surface, 1, logo_x, logo_y);

    char str[256];
    int i = 0;
    int line_y = 50;
    int line_spacing = 24;

    ksnprintf(str, sizeof(str), "AmitX v%s (%s)", AMITX_VERSION, AMITX_CODENAME);
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(str)) / 2, line_y + line_spacing * i, str, gfx_theme_color(GFX_FG_TEXT));
    i++;

    ksnprintf(str, sizeof(str), "Ram: %dB", pmm_get_total_ram());
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(str)) / 2, line_y + line_spacing * i, str, gfx_theme_color(GFX_FG_TEXT));
    i++;

    ksnprintf(str, sizeof(str), "Build: %s", AMITX_BUILD_DATE);
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(str)) / 2, line_y + line_spacing * i, str, gfx_theme_color(GFX_FG_TEXT));
    i++;

    ksnprintf(str, sizeof(str), "Arch: x86");
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(str)) / 2, line_y + line_spacing * i, str, gfx_theme_color(GFX_FG_TEXT));
    i++;

    ksnprintf(str, sizeof(str), "Kernel: Monolithic");
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(str)) / 2, line_y + line_spacing * i, str, gfx_theme_color(GFX_FG_TEXT));
    i++;

    ksnprintf(str, sizeof(str), "Executable: AMX v1");
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(str)) / 2, line_y + line_spacing * i, str, gfx_theme_color(GFX_FG_TEXT));
    i++;

    ksnprintf(str, sizeof(str), "File system: AmFS");
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(str)) / 2, line_y + line_spacing * i, str, gfx_theme_color(GFX_FG_TEXT));
    i++;

    /* OK button */
    gfx_button(&win->surface, (ABOUT_W - 100) / 2, ABOUT_H - 50, 100, 22, "OK", 0);

    const char *tip = " [q/esc] Back  [A] Authors ";
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(tip)) / 2, ABOUT_H - 24, tip, gfx_theme_color(GFX_FG_TEXT));
}

/* ==========================================================================
 * Draw authors view
 * ======================================================================= */
static void about_draw_authors(void) {
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);

    gfx_desktop(&win->surface);

    gfx_panel(&win->surface, 8, 8, ABOUT_W - 16, ABOUT_H - 16, bg);
    gfx_bevel_in(&win->surface, 8, 8, ABOUT_W - 16, ABOUT_H - 16);
    gfx_title_bar(&win->surface, 8, 8, ABOUT_W - 16, " About ");

    char str[256];

    ksnprintf(str, sizeof(str), "Authors:");
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(str)) / 2, 40, str, gfx_theme_color(GFX_FG_TEXT));

    ksnprintf(str, sizeof(str), "Amity");
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(str)) / 2, 80, str, gfx_theme_color(GFX_FG_TEXT));

	/* Some day people can add their entries here
	 * ksnprintf(str, sizeof(str), "Amity");
	 * window_draw_text(about, (fb.back.width - gfx_get_string_width(str))/2, 80 + 16 * i, str, gfx_theme_color(GFX_FG_TEXT));
	*/

    const char *tip = " [q/esc] Back ";
    window_draw_text(about, (ABOUT_W - gfx_get_string_width(tip)) / 2, ABOUT_H - 24, tip, gfx_theme_color(GFX_FG_TEXT));
}

/* ==========================================================================
 * Main loop
 * ======================================================================= */
void about_run(void) {
    int win_x = (fb.back.width - ABOUT_W) / 2;
    int win_y = (fb.back.height - ABOUT_H) / 2;

    about = window_create(win_x, win_y, ABOUT_W, ABOUT_H, "About", WIN_FLAG_TITLEBAR);
    if ((int)about == WIN_INVALID) {
        printk("[about] Failed to create window\n");
        return;
    }

    win = window_get(about);
    if (!win) {
        printk("[about] Failed to get window\n");
        return;
    }

    int running = 1;
    int showing_authors = 0;

    about_draw();
    compositor_render();

    while (running != 0) {
        /* Check OK button — draw + test click each frame */
        if (ui_button(&win->surface, (ABOUT_W - 100) / 2, ABOUT_H - 50, 100, 22, "OK")) {
            running = 0;
        }

        unsigned char c;
        if (keyboard_poll(&c)) {
            switch (c) {
            case 'q':
            case KEY_ESC:
                if (showing_authors) {
                    showing_authors = 0;
                    about_draw();
                    compositor_render();
                } else {
                    running = 0;
                }
                break;

            case 'A':
                if (!showing_authors) {
                    showing_authors = 1;
                    about_draw_authors();
                    compositor_render();
                }
                break;
            }
        }

        /* Re-present each frame for button hover/press feedback */
        if (!showing_authors) {
            about_draw();
            if (ui_button(&win->surface, (ABOUT_W - 100) / 2, ABOUT_H - 50, 100, 22, "OK")) {
                running = 0;
            }
            compositor_render();
        }
    }

    window_destroy(about);
}