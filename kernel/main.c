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
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

/* --- Macros ---*/

/* --- Includes ---*/
/* initialize with KScope delete soon*/
#include <fs/amfs.h>
#include <gfx/fb.h>
#include <hw/svga.h>

/* these can stay */
#include <arch/x86/gdt.h>
#include <arch/x86/io.h>
#include <arch/x86/time.h>
#include <arch/x86/timer.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/serial.h>
#include <gfx/compositor.h>
#include <gfx/window.h>
#include <hw/acpi.h>
#include <hw/e1000.h>
#include <internal/multiboot.h>
#include <internal/phonon_consts.h>
#include <internal/virtmem.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <mm/paging.h>
#include <mm/vmm.h>
#include <screen/printk.h>
#include <screen/screen.h>
#include <shell/cyclone.h>
#include <stdint.h>
#include <ui/menu.h>
/* --- Typedefs - Structs - Enums ---*/
/* --- Globals ---*/
int owly;
extern volatile uint32_t tick_count;

/* --- Prototypes ---*/

/* --- Functions ---*/

void system_shutdown(void) { acpi_shutdown(); }

void system_reboot(void) { acpi_reboot(); }

void launch_app(int app_code) {
  outb(PORT_QEMU_EXIT, QEMU_APP_BASE + app_code);
}

void draw_start(void) {
  move_cursor(0, 0);
  setcolor(15, 0);
  puts("Hello from the AmitX Kernel\n");
  draw_statusbar();
  puts("\n");
  draw_list(0, 2, 20, 2 + main_menu_count, main_menu, main_menu_count, POINTER);
  move_cursor(40, 12);
  reset_mouse_cursor_state();
  draw_mouse_cursor();
}

extern uint32_t multiboot_info_ptr;
void kernel_main(void) {
  kernel_setup();

  menu_run();
}