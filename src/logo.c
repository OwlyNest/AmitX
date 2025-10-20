
#include "screen.h"
#include "logo.h"
#include "amitx_info.h"

void draw_logo1() {
    move_cursor(0, 2);
    puts("     ___\n");
    puts("    (o,o)\n");
    puts("    {\" \"}\n");
    puts("     \" \" \n");
}
void draw_logo2() {
    move_cursor(0, 2);
    puts("    ____________\n");
    puts("   / ____  ____ \\\n");
    puts("  / / @  \\/ @  \\ \\\n");
    puts("  \\ \\____/\\____/ /\\\n");
    puts("   \\_____\\/_____/||\n");
    puts("   /       /\\\\\\\\\\//\n");
    puts("   |0xC0FFEE\\\\\\\\\\\\\n");
    puts("   \\        \\\\\\\\\\\\\n");
    puts("    \\________/\\\\\\\\\n");
    puts("      _||_||_   \\\\\n");
    puts("       -- --     \\\n");
}

void draw_logo(int version) {
    
    switch (version) {
        case 1:
            draw_logo1();
            break;
        case 2:
            draw_logo2();
            break;
        default:
            draw_logo1();
            break;
    }
    
}
