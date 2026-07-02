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
#include <internal/amitx_info.h>
#include <screen/printk.h>
#include <ui/menu.h>
#include <ui/system_manager.h>
#include <ui/device_manager.h>
#include <ui/about.h>
#include <kernel/kernel.h>
#include <drivers/keyboard.h>
#include <gfx/gfx_screen.h>
#include <gfx/fb.h>
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
 * Draw the main menu screen
 * ======================================================================= */
void gfx_menu_draw(void) {
    gfx_desktop();
    gfx_logo_design2(100, 60);

    int menu_width = 400;
    int menu_height = 280;
    int menu_x = (fb.back.width - menu_width) / 2;
    int menu_y = (fb.back.height - menu_height) / 2;

    gfx_panel(menu_x, menu_y, menu_width, menu_height, gfx_theme_color(GFX_BG_PANEL));
    gfx_bevel_in(menu_x, menu_y, menu_width, menu_height);
    gfx_title_bar(menu_x, menu_y, menu_width, " AmitX Main Menu ");

    gfx_list(menu_x + 4, menu_y + 28, menu_width - 8, menu_height - 32, main_menu, main_menu_count, POINTER);

    char text[64];
    const char *version = AMITX_VERSION;
    const char *date = AMITX_BUILD_DATE;
    ksnprintf(text, sizeof(text), "AmitX OS v%s (%s)", version, date);
    gfx_status_bar(0, fb.back.height - 24, fb.back.width, text);

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
        case 4: about_run(); break;
        case 5: system_reboot(); break;
        case 6: system_shutdown(); break;
        default: break;
    }

    menu = 1;
    gfx_desktop();
    gfx_menu_draw();
}