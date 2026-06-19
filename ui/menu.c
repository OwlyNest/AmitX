/*
	* ui/menu.c - [Enter description]
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
#include <ui/menu.h>
#include <ui/system_manager.h>
#include <ui/device_manager.h>
#include <kernel/kernel.h>
#include <drivers/keyboard.h>
#include <screen/screen.h>
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
void menu_select(int choise);
/* --- Functions ---*/
void menu_run(void) {
	clear();
	menu = 1;
	draw_start();

	while (menu) {
		unsigned char c = keyboard_getchar();

		if (c == 's' || c == KEY_DOWN) {
			if (POINTER < main_menu_count - 1) POINTER++;
			draw_start();
		} else if (c == 'w' || c == KEY_UP) {
			if (POINTER > 0) POINTER--;
			draw_start();
		} else if (c == '\n') {
			menu_select(POINTER);
		}
	}
}

void menu_select(int choise) {
	menu = 0;
	clear();
	switch (choise) {
		case 0: device_manager_run(); break;
		case 1: system_manager_run(); break;
		case 2: break;
		case 3: load_cyclone = 1; cyclone_main(1); break;
		case 4: system_reboot(); break;
		case 5: system_shutdown(); break;
	}

	menu = 1;
	clear();
	draw_start();
}