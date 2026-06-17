#include <shell/cyclone.h>
#include <screen/screen.h>
#include <drivers/keyboard.h>
#include <arch/x86/time.h>
#include <logo/logo.h>
#include <shell/commands.h>
#include <lib/string.h>

extern int menu;
extern int load_cyclone;
int version = 1;

void cyclone_main(int first) {
    (void)first;  /* no longer needed — kernel flushes buffer before entry */
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
            unsigned char c = keyboard_getchar();

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
            } else if (c >= 32 && c < 128 && pos < sizeof(input) - 1) {
                input[pos++] = c;
                putc(c);
                next_white();
            }
        }
    }
}