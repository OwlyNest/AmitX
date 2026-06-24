/*
	* include/drivers/fb.h - [Enter description]
	* Author:   amity
	* Date:     Tue Jun 23 13:13:21 2026
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
#ifndef FB_H
#define FB_H

#define FB_RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define FB_RGBA(r, g, b, a) (((uint32_t)(a) << 24) | FB_RGB(r, g, b))
/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
typedef struct fb_surface {
    uint32_t *pixels;      /* Current back buffer */
    uint32_t *front;       /* Front buffer (mapped FB) */
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;       /* bytes per line */
    uint32_t  pitch_px;    /* pixels per line (pitch/4) */
    int       initialized;
} fb_surface_t;
/* --- Globals ---*/
extern fb_surface_t fb;
/* --- Prototypes ---*/
int  fb_init(void);
void fb_present(void);                    /* Copy backbuffer → screen + update */
void fb_clear(uint32_t color);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void fb_draw_circle(int cx, int cy, int radius, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color); /* outline */
#endif