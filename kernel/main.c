/*
	* kernel/main.c - [Enter description]
	* Author:   amity
	* Date:     Wed Jun 10 10:32:19 2026
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
#include "screen.h"
#include "keyboard.h"
#include "mouse.h"
#include "kernel.h"
#include "io.h"
#include "acpi.h"
#include "cyclone.h"
#include "amitx_consts.h"
#include "menu.h"
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
/* --- Globals ---*/
int owly;
/* --- Prototypes ---*/

/* --- Functions ---*/

void system_shutdown(void) {
    acpi_shutdown();
}

void system_reboot(void) {
    acpi_reboot();
}

void launch_app(int app_code) {
    outb(PORT_QEMU_EXIT, QEMU_APP_BASE + app_code);
}

void draw_start(void) {
    move_cursor(0, 0);
    setcolor(15, 0);
    puts("Hello from the AmitX Kernel\n");
    draw_statusbar();
    puts("\n");
    draw_list(0, 2, 20, 7, main_menu, main_menu_count, POINTER);
    move_cursor(40, 12);
    reset_mouse_cursor_state();
    draw_mouse_cursor();
}

void kernel_main(void) {
    kernel_setup();
    menu_run();
}