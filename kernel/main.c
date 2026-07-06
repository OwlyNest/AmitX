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
/* initialize with KScope delete soon*/
#include <fs/amfs.h>
#include <hw/svga.h>
#include <gfx/fb.h>

/* these can stay */
#include <screen/screen.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <kernel/kernel.h>
#include <mm/heap.h>
#include <gfx/compositor.h>
#include <drivers/serial.h>
#include <screen/printk.h>
#include <arch/x86/io.h>
#include <arch/x86/gdt.h>
#include <lib/string.h>
#include <arch/x86/timer.h>
#include <arch/x86/time.h>
#include <hw/acpi.h>
#include <hw/e1000.h>
#include <gfx/window.h>
#include <mm/vmm.h>
#include <mm/paging.h>
#include <internal/virtmem.h>
#include <kernel/syscall.h>
#include <shell/cyclone.h>
#include <internal/amitx_consts.h>
#include <ui/menu.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
/* --- Globals ---*/
int owly;
extern volatile uint32_t tick_count;

/* --- Prototypes ---*/

/* --- Functions ---*/

void ring3_hello(void) {
    syscall(SYS_WRITE, (uint32_t)"Hello from ring 3!\n", 0, 0);
    while (1);
}

void test_ring3(void) {
    printk("[test] Jumping to ring 3...\n");
    uint32_t* user_stack = (uint32_t*)malloc(4096);
    uint32_t user_esp = (uint32_t)(user_stack + 1024);
    usermode_jump((uint32_t)ring3_hello, user_esp);
}

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
    draw_list(0, 2, 20, 2 + main_menu_count, main_menu, main_menu_count, POINTER);
    move_cursor(40, 12);
    reset_mouse_cursor_state();
    draw_mouse_cursor();
}

void kernel_main(void) {
    kernel_setup();

    void *v = vmm_map_physical(0x000B8000, 16, PAGE_WRITABLE);
    printk("[test] vmm@0xB8000: %02x %02x %02x %02x\n",((uint8_t*)v)[0], ((uint8_t*)v)[1], ((uint8_t*)v)[2], ((uint8_t*)v)[3]);
    printk("[test] PD[1016]=0x%08x PT[0]=0x%08x\n", PD_VIRT[1016], PT_VIRT(1016)[0]);

    menu_run();
}