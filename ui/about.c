/*
	* ui/about.c - [Enter description]
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
#define ABOUT_X        (fb.back.width-ABOUT_W)/2
#define ABOUT_Y        (fb.back.height-ABOUT_H)/2
/* --- Includes ---*/
#include <ui/about.h>
#include <gfx/gfx_screen.h>
#include <gfx/fb.h>
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

/* --- Prototypes ---*/

/* --- Functions ---*/

static void about_draw(void) {
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);

    gfx_desktop();

    gfx_panel(ABOUT_X, ABOUT_Y, ABOUT_W, ABOUT_H, bg);
    gfx_bevel_in(ABOUT_X, ABOUT_Y, ABOUT_W, ABOUT_H);
    gfx_title_bar(ABOUT_X, ABOUT_Y, ABOUT_W, " About ");
	draw_logo_gfx(1, ((fb.back.width + ABOUT_W)/2)-75 - 5, (fb.back.height - ABOUT_H)/2 + 20 + 5);

	char str[256]; /* Just reuse this one */
	int i = 0;
	int free_lines = 3;

	ksnprintf(str, sizeof(str), "AmitX v%s (%s)", AMITX_VERSION, AMITX_CODENAME);
	gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 8 * free_lines * i, str, gfx_theme_color(GFX_FG_TEXT));
	i++;

	ksnprintf(str, sizeof(str), "Ram: %dB", pmm_get_total_ram());
	gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 8 * free_lines * i, str, gfx_theme_color(GFX_FG_TEXT));
	i++;

	ksnprintf(str, sizeof(str), "Build: %s", AMITX_BUILD_DATE);
	gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 8 * free_lines * i, str, gfx_theme_color(GFX_FG_TEXT));
	i++;
	
	/* Un hardcode these later */
	ksnprintf(str, sizeof(str), "Arch: x86");
	gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 8 * free_lines * i, str, gfx_theme_color(GFX_FG_TEXT));
	i++;

	ksnprintf(str, sizeof(str), "Kernel: Monolithic");
	gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 8 * free_lines * i, str, gfx_theme_color(GFX_FG_TEXT));
	i++;

	ksnprintf(str, sizeof(str), "Executable: AMX v1");
	gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 8 * free_lines * i, str, gfx_theme_color(GFX_FG_TEXT));
	i++;

	ksnprintf(str, sizeof(str), "File system: AmFS");
	gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 8 * free_lines * i, str, gfx_theme_color(GFX_FG_TEXT));
	i++;

	const char *tip = " [q/esc] Back ";
    gfx_draw_text((fb.back.width - gfx_text_width(tip))/2, ABOUT_Y + ABOUT_H - 12, tip, gfx_theme_color(GFX_FG_TEXT));

    fb_present();
}

static void about_draw_authors(void) {
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);

    gfx_desktop();

    gfx_panel(ABOUT_X, ABOUT_Y, ABOUT_W, ABOUT_H, bg);
    gfx_bevel_in(ABOUT_X, ABOUT_Y, ABOUT_W, ABOUT_H);
    gfx_title_bar(ABOUT_X, ABOUT_Y, ABOUT_W, " About ");

	char str[256];
	int i = 1;

	ksnprintf(str, sizeof(str), "Authors:");
	gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 8, str, gfx_theme_color(GFX_FG_TEXT));

	
	ksnprintf(str, sizeof(str), "Amity");
	gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 32 + 8 * i, str, gfx_theme_color(GFX_FG_TEXT));
	i++;
	/* Some day people can add their entries here
	 * ksnprintf(str, sizeof(str), "Amity");
	 * gfx_draw_text((fb.back.width - gfx_text_width(str))/2, ABOUT_Y + 24 + 32 + 8 * i, str, gfx_theme_color(GFX_FG_TEXT));
	 * i++;
	*/

    const char *tip = " [q/esc] Back ";
    gfx_draw_text((fb.back.width - gfx_text_width(tip))/2, ABOUT_Y + ABOUT_H - 12, tip, gfx_theme_color(GFX_FG_TEXT));

    fb_present();
}

/* ==========================================================================
 * Main loop
 * ======================================================================= */
void about_run(void) {
	about_draw();
	int running = 1;
	gfx_button(ABOUT_X + 150, ABOUT_Y + 300, 100, 22, "OK", 0);
	fb_present(); /* Just draw the button, don't check, presenting every loop makes the mouse have an epeleptic attack */

    while (running != 0) {
		if (ui_button(ABOUT_X + 150, ABOUT_Y + 300, 100, 22, "OK")) {
			running = 0;
		}
		unsigned char c;

		if (keyboard_poll(&c)) {

			switch (c) {

			case 'q':
			case KEY_ESC:
				running = 0;
				break;

			case 'A':
				about_draw_authors();
				break;
			}
		}
    }
}