
#include "cyclone.h"
#include "kernel.h"
#include "screen.h"
#include "keyboard.h"
#include "time.h"
#include "logo.h"
#include "commands.h"
#include "string.h"

extern int menu;
extern int load_cyclone;
int version = 1;

void cyclone_main(int first) {
    menu = 0;
    clear();
    
    puts("Cyclone REPL v0.1\nType 'help' for commands\n\n");
    draw_logo(version);
    newline();

    char input[128];
    size_t pos = 0;

    while (1) {
        if (load_cyclone == 0) {
            break;
        }
        puts("> ");
        next_white();
        pos = 0;

        while (1) {
            char c = keyboard_getchar();

            if (first) {
                putc('\b');
                putc('\b');
                break;
            }

            if (c == '\n') {
                input[pos] = '\0';
                newline();
                execute_command(input);
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    putc('\b');
                    putc(' ');
                    putc('\b');
                }
            } else if (c >= 32 && pos < sizeof(input) - 1) {
                input[pos++] = c;
                putc(c);
                next_white();
            }
        }
        first = 0;
    }
}
