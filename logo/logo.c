#include <screen/screen.h>
#include <logo/logo.h>
#include <gfx/fb.h>

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

/* ==========================================================================
 * Logo 1: Simple Owly (minimalist) (75x78)
 * ======================================================================= */
static void draw_pixel_owl1(gfx_surface_t *surface, int x, int y) {
    /* Body — brown rectangle */
    gfx_fill_rect(surface, x + 20, y + 25, 40, 45, fb_pack_pixel(139, 90, 43));
    
    /* Head — lighter brown */
    gfx_fill_rect(surface, x + 15, y + 5, 50, 30, fb_pack_pixel(160, 120, 70));
    
    /* Ear tufts */
    gfx_fill_rect(surface, x + 20, y, 10, 10, fb_pack_pixel(139, 90, 43));
    gfx_fill_rect(surface, x + 50, y, 10, 10, fb_pack_pixel(139, 90, 43));
    
    /* Eyes — white with black pupils */
    gfx_fill_rect(surface, x + 25, y + 12, 12, 12, fb_pack_pixel(255, 255, 255));
    gfx_fill_rect(surface, x + 43, y + 12, 12, 12, fb_pack_pixel(255, 255, 255));
    gfx_fill_rect(surface, x + 29, y + 16, 4, 4, fb_pack_pixel(0, 0, 0));
    gfx_fill_rect(surface, x + 47, y + 16, 4, 4, fb_pack_pixel(0, 0, 0));
    
    /* Beak — orange */
    gfx_fill_rect(surface, x + 34, y + 24, 12, 8, fb_pack_pixel(255, 165, 0));
    
    /* Wings — darker brown */
    gfx_fill_rect(surface, x + 5, y + 30, 15, 35, fb_pack_pixel(100, 65, 30));
    gfx_fill_rect(surface, x + 60, y + 30, 15, 35, fb_pack_pixel(100, 65, 30));
    
    /* Feet — orange */
    gfx_fill_rect(surface, x + 25, y + 70, 12, 8, fb_pack_pixel(255, 165, 0));
    gfx_fill_rect(surface, x + 43, y + 70, 12, 8, fb_pack_pixel(255, 165, 0));
    
    /* Belly — lighter */
    gfx_fill_rect(surface, x + 30, y + 40, 20, 25, fb_pack_pixel(180, 150, 120));
}

/* ==========================================================================
 * Logo 2: Detailed owl with coffee cup
 * ======================================================================= */
static void draw_pixel_owl2(gfx_surface_t *surface, int x, int y) {
    /* Body — rich brown */
    gfx_fill_rect(surface, x + 20, y + 30, 50, 50, fb_pack_pixel(120, 80, 40));
    
    /* Head */
    gfx_fill_rect(surface, x + 15, y + 10, 60, 30, fb_pack_pixel(140, 100, 60));
    
    /* Ear tufts — pointed */
    gfx_fill_rect(surface, x + 20, y + 2, 8, 12, fb_pack_pixel(120, 80, 40));
    gfx_fill_rect(surface, x + 62, y + 2, 8, 12, fb_pack_pixel(120, 80, 40));
    
    /* Eyes — large, expressive */
    gfx_fill_rect(surface, x + 24, y + 16, 16, 14, fb_pack_pixel(255, 255, 255));
    gfx_fill_rect(surface, x + 50, y + 16, 16, 14, fb_pack_pixel(255, 255, 255));
    /* Pupils — looking slightly right */
    gfx_fill_rect(surface, x + 32, y + 20, 6, 6, fb_pack_pixel(0, 0, 0));
    gfx_fill_rect(surface, x + 54, y + 20, 6, 6, fb_pack_pixel(0, 0, 0));
    /* Eye shine */
    fb_put_pixel((uint32_t)(x + 34), (uint32_t)(y + 22), fb_pack_pixel(255, 255, 255));
    fb_put_pixel((uint32_t)(x + 56), (uint32_t)(y + 22), fb_pack_pixel(255, 255, 255));
    
    /* Beak */
    gfx_fill_rect(surface, x + 38, y + 28, 14, 10, fb_pack_pixel(255, 140, 0));
    
    /* Wings — folded */
    gfx_fill_rect(surface, x + 5, y + 35, 15, 40, fb_pack_pixel(90, 60, 30));
    gfx_fill_rect(surface, x + 70, y + 35, 15, 40, fb_pack_pixel(90, 60, 30));
    
    /* Wing detail lines */
    fb_draw_line(x + 8, y + 45, x + 8, y + 70, fb_pack_pixel(60, 40, 20));
    fb_draw_line(x + 12, y + 45, x + 12, y + 70, fb_pack_pixel(60, 40, 20));
    fb_draw_line(x + 73, y + 45, x + 73, y + 70, fb_pack_pixel(60, 40, 20));
    fb_draw_line(x + 77, y + 45, x + 77, y + 70, fb_pack_pixel(60, 40, 20));
    
    /* Feet */
    gfx_fill_rect(surface, x + 28, y + 80, 12, 8, fb_pack_pixel(255, 140, 0));
    gfx_fill_rect(surface, x + 50, y + 80, 12, 8, fb_pack_pixel(255, 140, 0));
    
    /* Belly patch */
    gfx_fill_rect(surface, x + 30, y + 45, 30, 30, fb_pack_pixel(200, 180, 160));
    
    /* Coffee cup — held in right wing */
    gfx_fill_rect(surface, x + 72, y + 55, 18, 22, fb_pack_pixel(220, 220, 220));
    /* Cup rim */
    gfx_fill_rect(surface, x + 70, y + 52, 22, 6, fb_pack_pixel(180, 180, 180));
    /* Coffee */
    gfx_fill_rect(surface, x + 74, y + 54, 14, 4, fb_pack_pixel(80, 40, 20));
    /* Steam */
    fb_draw_line(x + 78, y + 48, x + 80, y + 42, fb_pack_pixel(200, 200, 200));
    fb_draw_line(x + 82, y + 48, x + 84, y + 40, fb_pack_pixel(200, 200, 200));
    
    /* 0xC0FFEE on cup */
    fb_draw_string(x + 74, y + 62, "C0", fb_pack_pixel(80, 40, 20));
    fb_draw_string(x + 74, y + 70, "FF", fb_pack_pixel(80, 40, 20));
}

/* ==========================================================================
 * Public draw function
 * ======================================================================= */
void draw_logo_gfx(gfx_surface_t *surface, int version, int x, int y) {
    if (version == 2) {
        draw_pixel_owl2(surface, x, y);
    } else {
        draw_pixel_owl1(surface, x, y);
    }
}