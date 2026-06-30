/*
    * shell/commands.c - Cyclone command handlers (graphics mode)
    * Author:   amity
    * Date:     Thu Jun 25 11:10:00 2026
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
#include <hw/rtc.h>
#include <fs/amfs.h>
#include <ui/text_editor.h>
#include <shell/commands.h>
#include <shell/cyclone.h>
#include <gfx/gfx_term.h>
#include <gfx/gfx_screen.h>
#include <screen/printk.h>
#include <gfx/fb.h>
#include <logo/logo.h>
#include <mm/heap.h>
#include <shell/utils.h>
#include <tests/tests.h>
#include <kernel/kernel.h>
#include <arch/x86/time.h>
#include <lib/string.h>
#include <exec/amx.h>
#include <exec/loader.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern int tick_count;
extern int load_cyclone;
extern int version;

/* --- Prototypes ---*/
void cmd_ls(void);
void run_script(const char *path);
/* --- Functions ---*/

/* ==========================================================================
 * Command dispatcher
 * ======================================================================= */
void execute_command(const char* input) {
    gfx_term_puts("[::]:\n");
    
    if (starts_with(input, "echo ") || starts_with(input, "hoot ")) {
        const char* message = input + 5;
        gfx_term_puts(message);
    } else if (starts_with(input, "mkdir")) {
        const char *path = input + 6;
        amfs_mkdir(path);
    } else if (starts_with(input, "hex ")) {
        const char* num = input + 4;
        uint32_t number = atoi(num);
        gfx_term_puthex(number);
    } else if (starts_with(input, "test ")) {
        const char* num = input + 5;
        int n = atoi(num);
        test(n);
    } else if (strcmp(input, "coffee") == 0) {
        uint32_t number = 12648430;
        gfx_term_puthex(number);
    } else if (strcmp(input, "time") == 0) {
        rtc_time_t t;
        rtc_read(&t);
        printk("Date: %d-%d-%d %d:%d:%d", t.year, t.month, t.day, t.hour, t.minute, t.second);
    } else if (strcmp(input, "back") == 0) {
        load_cyclone = 0;
    } else if (starts_with(input, "load")) {
        const char *path = input + 5;
        exec_run(path);
    } else if (starts_with(input, "ls")) {
        const char* dir = input + 3;
        amfs_ls(dir);
    } else if (starts_with(input, "amity")) {
        const char *path = input + 6;
        amity_run(path);
        /* Redraw terminal after amity exits */
        gfx_term_clear();
        gfx_term_draw_frame(" Cyclone REPL v0.9");
        draw_logo_gfx(version, 700, 60);
        gfx_term_newline();
    } else if (starts_with(input, "cat")) {
        const char *path = input + 4;
        amfs_cat(path);
    } else if (starts_with(input, "run")) {
        const char *path = input + 4;
        run_script(path);
    } else if (strcmp(input, "switch logo") == 0) {
        version = (version == 1) ? 2 : 1;
        /* Erase old logo area and redraw */
        gfx_fill_rect(700, 60, 100, 100, gfx_theme_color(GFX_BG_PANEL));
        draw_logo_gfx(version, 700, 60);
    } else if (strcmp(input, "help") == 0) {
        gfx_term_puts("Available commands:");
        gfx_term_newline();
        gfx_term_puts("  echo/hoot <text>   - Print text");
        gfx_term_newline();
        gfx_term_puts("  hex <number>       - Print number as hex");
        gfx_term_newline();
        gfx_term_puts("  time               - Show uptime");
        gfx_term_newline();
        gfx_term_puts("  clear              - Clear terminal");
        gfx_term_newline();
        gfx_term_puts("  back               - Return to menu");
        gfx_term_newline();
        gfx_term_puts("  quit               - Exit system");
        gfx_term_newline();
        gfx_term_puts("  coffee             - Print 0xC0FFEE");
        gfx_term_newline();
        gfx_term_puts("  ls <dir>           - Print files");
        gfx_term_newline();
        gfx_term_puts("  switch logo        - Switch Owly art");
    } else if (strcmp(input, "quit") == 0) {
        sleep(1);
        system_reboot();
    } else if (strcmp(input, "clear") == 0) {
        gfx_term_clear();
        draw_logo_gfx(version, 700, 60);
    } else {
        gfx_term_puts("Unknown command");
    }
    gfx_term_newline();
}

void run_script(const char *path) {
    char buf[4096];
    int len = amfs_read(path, buf, sizeof(buf) - 1);
    if (len <= 0) return;
    buf[len] = '\0';

    char line[128];
    int line_pos = 0;

    for (int i = 0; i <= len; i++) {
        if (buf[i] == '\n' || buf[i] == '\0') {
            if (line_pos > 0) {
                line[line_pos] = '\0';
                execute_command(line);  /* Your existing Cyclone dispatcher */
                line_pos = 0;
            }
        } else if (line_pos < 127) {
            line[line_pos++] = buf[i];
        }
    }
}