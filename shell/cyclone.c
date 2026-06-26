/*
    * shell/cyclone.c - Cyclone REPL (graphics mode)
    * Author:   amity
    * Date:     Thu Jun 25 11:35:00 2026
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
#include <shell/cyclone.h>
#include <shell/commands.h>
#include <drivers/gfx_term.h>
#include <drivers/gfx_screen.h>
#include <drivers/fb.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <logo/logo.h>
#include <stddef.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern int menu;
extern int load_cyclone;
int version = 1;

/* --- Prototypes ---*/

/* --- Functions ---*/

/* ==========================================================================
 * Main Cyclone REPL loop
 * ======================================================================= */
void cyclone_main(int first) {
    (void)first;
    menu = 0;
    load_cyclone = 1;
    
    /* Init terminal: full screen with margins */
    gfx_term_init(20, 20, 984, 728,  gfx_theme_color(GFX_FG_TEXT), gfx_theme_color(GFX_BG_PANEL));
    gfx_term_draw_frame(" Cyclone REPL v0.1 ");
    
    /* Draw logo in the top-right area of the terminal */
    draw_logo_gfx(version, 700, 60);
    
    gfx_term_newline();
    gfx_term_puts("Type 'help' for commands");
    gfx_term_newline();
    gfx_term_newline();
    
    fb_present();
    
    char input[128];
    size_t pos = 0;
    
    while (load_cyclone) {
        gfx_term_draw_prompt();
        fb_present();
        
        pos = 0;
        input[0] = '\0';
        
        while (1) {
            unsigned char c = keyboard_getchar();
            
            if (c == '\n') {
                input[pos] = '\0';
                gfx_term_newline();
                execute_command(input);
                fb_present();
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    gfx_term_backspace();
                    fb_present();  /* show erased char */
                }
            } else if (c >= 32 && c < 128 && pos < sizeof(input) - 1) {
                input[pos++] = c;
                gfx_term_putc(c);
                fb_present();  /* periodic update */
            }
        }
    }
}