
#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define COLOR_THEME_DEFAULT 0x0F  // white on black
#define COLOR_THEME_STATUS  0x1F  // white on blue
#define COLOR_THEME_HIGHLIGHT 0xF0 // black on white


// VGA text rendering
void clear();
void putc(char c);
void setcolor(uint8_t fg, uint8_t bg);
void newline();
void draw_uptime();
void draw_statusbar(void);
void puts(const char* str);
void blink();
void putf(const char* str, uint8_t fg, uint8_t bg);
void putint(int num);
void puthex(uint32_t n);
void set_cursor(int x, int y);
void next_white();
void move_cursor(uint8_t x, uint8_t y);
void get_cursor(uint8_t* x, uint8_t* y);
int sputf(char* out, const char* format, ...);
void itoa_pad(int value, char* buffer, int width);

// GUI elements
void draw_box(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t fg, uint8_t bg);
void draw_title_box(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const char* title, uint8_t fg, uint8_t bg);
void draw_progress_bar(uint8_t x, uint8_t y, uint8_t width, uint8_t percent, uint8_t fg, uint8_t bg);
void draw_list(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const char* items[], uint8_t count, uint8_t selected);

// Mouse
void draw_mouse_cursor();
#endif
