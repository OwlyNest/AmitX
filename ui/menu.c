/*
	* ui/menu.c - Main menu (windowed)
	* Author:   amity
	* Date:     Wed Jun 10 12:20:47 2026
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
#define MENU_W      400
#define MENU_H      280

/* --- Includes ---*/
#include <internal/phonon_info.h>
#include <screen/printk.h>
#include <ui/menu.h>
#include <gfx/compositor.h>
#include <ui/system_manager.h>
#include <ui/device_manager.h>
#include <ui/about.h>
#include <kernel/kernel.h>
#include <drivers/keyboard.h>
#include <gfx/window.h>
#include <gfx/fb.h>
#include <drivers/mouse.h>
#include <internal/phonon_consts.h>
#include <shell/cyclone.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
int POINTER = 0;
int load_cyclone = 0;
int menu = 0;

window_handle_t men;
static window_t *win;

const char* main_menu[] = {
    "Device Manager",
    "System Manager",
    "Settings",
    "Cyclone",
    "About",
    "Reboot",
    "Shutdown"
};

const int main_menu_count = sizeof(main_menu) / sizeof(main_menu[0]);

/* --- Prototypes ---*/
void menu_select(int choice);
void gfx_menu_draw(void);

/* --- Functions ---*/

/* ==========================================================================
 * Draw the main menu into the window surface
 * All coordinates are window-local (0,0 = top-left of window)
 * ======================================================================= */
void gfx_menu_draw(void) {
    /* Clear window to desktop color */
    gfx_desktop(&win->surface);

    /* Menu panel centered in window */
    int panel_x = 20;
    int panel_y = 20;
    int panel_w = MENU_W - 40;
    int panel_h = MENU_H - 110;

    gfx_panel(&win->surface, panel_x, panel_y, panel_w, panel_h, gfx_theme_color(GFX_BG_PANEL));
    gfx_bevel_in(&win->surface, panel_x, panel_y, panel_w, panel_h);
    gfx_title_bar(&win->surface, panel_x, panel_y, panel_w, " AmitX Main Menu ");

    gfx_list(&win->surface, panel_x + 4, panel_y + 24, panel_w - 8, panel_h - 28, main_menu, main_menu_count, POINTER);

    /* Status bar at bottom of window */
    char text[64];
    const char *version = PHONON_VERSION;
    const char *date = PHONON_BUILD_DATE;
    ksnprintf(text, sizeof(text), "AmitX OS v%s (%s)", version, date);
    gfx_status_bar(&win->surface, 0, MENU_H - 24, MENU_W, text);
}

/* ==========================================================================
 * Main menu loop
 * ======================================================================= */
void menu_run(void) {
    int win_x = (fb.back.width - MENU_W) / 2;
    int win_y = (fb.back.height - MENU_H) / 2;

    men = window_create(win_x, win_y, MENU_W, MENU_H, "AmitX Main Menu", WIN_FLAG_BORDER | WIN_FLAG_TITLEBAR);
    if ((int)men == WIN_INVALID) {
        printk("[menu] Failed to create window\n");
        return;
    }

    win = window_get(men);
    if (!win) {
        printk("[menu] Failed to get window\n");
        return;
    }

    menu = 1;
    POINTER = 0;

    gfx_menu_draw();
    compositor_render();

    while (1) {
        unsigned char c = keyboard_getchar();

        if (c == 's' || c == KEY_DOWN) {
            if (POINTER < main_menu_count - 1) {
                POINTER++;
                gfx_menu_draw();
                compositor_render();
            }
        } else if (c == 'w' || c == KEY_UP) {
            if (POINTER > 0) {
                POINTER--;
                gfx_menu_draw();
                compositor_render();
            }
        } else if (c == '\n') {
            menu_select(POINTER);
            /* After submenu returns, redraw and present */
            gfx_menu_draw();
            compositor_render();
        }
    }

    window_destroy(men);
}

/* ==========================================================================
 * Handle menu selection
 * ======================================================================= */
void menu_select(int choice) {
    menu = 0;

    switch (choice) {
        case 0: device_manager_run(); break;
        case 1: system_manager_run(); break;
        case 2: break;
        case 3: load_cyclone = 1; cyclone_main(1); break;
        case 4: about_run(); break;
        case 5: system_reboot(); break;
        case 6: system_shutdown(); break;
        default: break;
    }

    menu = 1;
}