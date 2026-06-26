/*
	* ui/menu.c - Main menu
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

/* --- Includes ---*/
#include "internal/amitx_info.h"
#include "screen/printk.h"
#include <ui/menu.h>
#include <ui/system_manager.h>
#include <ui/device_manager.h>
#include <kernel/kernel.h>
#include <drivers/keyboard.h>
#include <drivers/gfx_screen.h>
#include <drivers/fb.h>
#include <drivers/mouse.h>
#include <internal/amitx_consts.h>
#include <shell/cyclone.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
int POINTER = 0;
int load_cyclone = 0;
int menu = 0;

const char* main_menu[] = {
    "Device Manager",
    "System Manager",
    "Settings",
    "Cyclone",
    "Reboot",
    "Shutdown"
};

const int main_menu_count = sizeof(main_menu) / sizeof(main_menu[0]);

/* --- Prototypes ---*/
void menu_select(int choice);
void gfx_menu_draw(void);

/* --- Functions ---*/

/* ==========================================================================
 * Draw the main menu screen
 * ======================================================================= */
void gfx_menu_draw(void) {
    gfx_desktop();
    gfx_logo_design2(100, 60);

    int mx = 312, my = 260, mw = 400, mh = 280;

    gfx_panel(mx, my, mw, mh, gfx_theme_color(GFX_BG_PANEL));
    gfx_bevel_in(mx, my, mw, mh);
    gfx_title_bar(mx, my, mw, " AmitX Main Menu ");

    gfx_list(mx + 4, my + 28, mw - 8, mh - 32, main_menu, main_menu_count, POINTER);

    char text[64];
    const char *version = AMITX_VERSION;
    const char *date = AMITX_BUILD_DATE;
    ksnprintf(text, sizeof(text), "AmitX OS v%s (%s)", version, date);
    gfx_status_bar(0, 744, 1024, text);

    fb_present();
}

/* ==========================================================================
 * Main menu loop
 * ======================================================================= */
void menu_run(void) {
    menu = 1;
    POINTER = 0;
    gfx_menu_draw();

    while (menu) {
        unsigned char c = keyboard_getchar();

        if (c == 's' || c == KEY_DOWN) {
            if (POINTER < main_menu_count - 1) {
                POINTER++;
                gfx_menu_draw();
            }
        } else if (c == 'w' || c == KEY_UP) {
            if (POINTER > 0) {
                POINTER--;
                gfx_menu_draw();
            }
        } else if (c == '\n') {
            menu_select(POINTER);
        } else if (c == 'q' || c == KEY_ESC) {
            break;
        }
    }
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
        case 4: system_reboot(); break;
        case 5: system_shutdown(); break;
    }

    menu = 1;
    gfx_desktop();
    gfx_menu_draw();
}