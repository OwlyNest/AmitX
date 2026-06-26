/*
    * screen.c - VGA text mode screen driver
    * Author:   amity
    * Date:     Wed Jun 10 10:32:00 2026
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
#include <screen/screen.h>
#include <internal/kscope.h>
#include <lib/string.h>
#include <internal/amitx_info.h>
#include <screen/printk.h>
#include <arch/x86/io.h>
#include <drivers/mouse.h>
#include <hw/pci.h>
#include <drivers/serial.h>
#include <internal/amitx_consts.h>
#include <internal/virtmem.h>
#include <drivers/keyboard.h>
#include <stdarg.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern int tick_count;

/* Use vga_memory() for auto-detect of physical vs virtual address */
uint16_t* video_memory = NULL;
uint8_t cursor_row = 0;
uint8_t cursor_col = 0;
uint8_t color = 0x0F;

/* --- Prototypes ---*/

/* --- Functions ---*/

/* ==========================================================================
 * Initialize video memory pointer (call before any screen operations)
 * ======================================================================= */
static int screen_init(void) {
    video_memory = vga_memory();
    return 0;
}

kscope_node_t screen_node = {
    .name = "vga-screen",
    .id = 0x0006,
    .class = KSCOPE_CLASS_UI,
    .subclass = KSCOPE_SUBCLASS_UI_SCREEN,
    .requires = NULL,
    .require_count = 0,
    .provides = (const char*[]){"gfx.screen", "io.printk", "debug.output"},
	.provide_count = 3,
    .init = screen_init,
};

void set_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;

    outb(PORT_VGA_CTRL, 14);
    outb(PORT_VGA_DATA, (pos >> 8) & 0xFF);

    outb(PORT_VGA_CTRL, 15);
    outb(PORT_VGA_DATA, pos & 0xFF);
}

void update_hardware_cursor() {
    set_cursor(cursor_col, cursor_row);
}

static void scroll_if_needed() {
    if (cursor_row < VGA_HEIGHT - 1) return;

    /* Scroll everything up one line */
    for (int row = 1; row < VGA_HEIGHT - 1; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            video_memory[(row - 1) * VGA_WIDTH + col] =
                video_memory[row * VGA_WIDTH + col];
        }
    }
    for (int col = 0; col < VGA_WIDTH; col++) {
        video_memory[(VGA_HEIGHT - 2) * VGA_WIDTH + col] =
            (color << 8) | ' ';
    }
    cursor_row = VGA_HEIGHT - 2;
}

void next_white() {
    video_memory[cursor_row * VGA_WIDTH + cursor_col] =
        (0x0F << 8) | CH_CURSOR;
}

void reset_mouse_cursor_state();

void clear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        video_memory[i] = (color << 8) | ' ';
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hardware_cursor();
    reset_mouse_position();
}

void putc(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
        update_hardware_cursor();
        return;
    }
    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            video_memory[cursor_row * VGA_WIDTH + cursor_col] =
                (color << 8) | CH_VERT;
            video_memory[cursor_row * VGA_WIDTH + cursor_col + 1] =
                (color << 8) | ' ';
            update_hardware_cursor();
        }
        return;
    }
    if (cursor_row >= VGA_HEIGHT - 1) {
        cursor_row = VGA_HEIGHT - 2;
        cursor_col = 0;
    }

    video_memory[cursor_row * VGA_WIDTH + cursor_col] = (color << 8) | c;
    cursor_col++;
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
        scroll_if_needed();
    }
    update_hardware_cursor();
}

void puts(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        putc(str[i]);
    }
}

void setcolor(uint8_t fg, uint8_t bg) {
    color = (bg << 4) | (fg & 0x0F);
}

void newline() {
    video_memory[cursor_row * VGA_WIDTH + cursor_col] = (0x0F << 8);
    cursor_col = 0;
    cursor_row++;
    scroll_if_needed();
    update_hardware_cursor();
}

void draw_statusbar() {
    const char *name = AMITX_NAME;
    const char *version = AMITX_VERSION;
    const char *build_date = AMITX_BUILD_DATE;

    char status_text[80];
    int pos = 0;

    while (*name && pos < 79) status_text[pos++] = *name++;
    if (pos < 79) status_text[pos++] = ' ';
    while (*version && pos < 79) status_text[pos++] = *version++;
    if (pos < 79) status_text[pos++] = ' ';
    if (pos < 79) status_text[pos++] = '(';
    while (*build_date && pos < 79) status_text[pos++] = *build_date++;
    if (pos < 79) status_text[pos++] = ')';
    status_text[pos] = '\0';

    const uint8_t status_bg = 1;    /* Blue background */
    const uint8_t status_fg = 15;   /* White foreground */
    uint8_t status_color = (status_bg << 4) | (status_fg & 0x0F);

    for (int col = 0; col < VGA_WIDTH; col++) {
        video_memory[(VGA_HEIGHT - 1) * VGA_WIDTH + col] =
            (status_color << 8) | ' ';
    }

    for (int i = 0; i < pos && i < VGA_WIDTH; i++) {
        video_memory[(VGA_HEIGHT - 1) * VGA_WIDTH + i] =
            (status_color << 8) | status_text[i];
    }
}

void putf(const char* str, uint8_t fg, uint8_t bg) {
    setcolor(fg, bg);
    puts(str);
    puts("\n");
}

void putint(int num) {
    char str[12];
    itoa(num, str);
    puts(str);
}

void puthex(uint32_t n) {
    puts("0x");
    char hex_chars[] = "0123456789ABCDEF";
    int started = 0;
    for (int i = 7; i >= 0; i--) {
        uint8_t nibble = (n >> (i * 4)) & 0xF;
        if (nibble != 0 || started || i == 0) {
            putc(hex_chars[nibble]);
            started = 1;
        }
    }
}

void draw_uptime() {
    uint8_t saved_x, saved_y;
    get_cursor(&saved_x, &saved_y);

    int seconds = tick_count / 100;
    move_cursor(0, 23);
    puts("Uptime: ");
    putint(seconds);
    puts("s");

    move_cursor(saved_x, saved_y);
}

void blink() {
    move_cursor(cursor_col, cursor_row);
    int seconds = tick_count / 100;
    int time = seconds % 2;
    if (!(time)) {
        setcolor(15, 0);
    } else {
        setcolor(0, 15);
    }
}

void move_cursor(uint8_t x, uint8_t y) {
    cursor_col = x;
    cursor_row = y;
    set_cursor(x, y);
}

void get_cursor(uint8_t* x, uint8_t* y) {
    *x = cursor_col;
    *y = cursor_row;
}

void draw_box(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
              uint8_t fg, uint8_t bg) {
    if (width < 2 || height < 2) return;

    uint8_t color_local = (bg << 4) | (fg & 0x0F);

    video_memory[y * VGA_WIDTH + x] = (color_local << 8) | CH_UL;
    video_memory[y * VGA_WIDTH + x + width - 1] =
        (color_local << 8) | CH_UR;
    video_memory[(y + height - 1) * VGA_WIDTH + x] =
        (color_local << 8) | CH_LL;
    video_memory[(y + height - 1) * VGA_WIDTH + x + width - 1] =
        (color_local << 8) | CH_LR;

    for (int i = 1; i < width - 1; i++) {
        video_memory[y * VGA_WIDTH + x + i] =
            (color_local << 8) | CH_HORIZ;
        video_memory[(y + height - 1) * VGA_WIDTH + x + i] =
            (color_local << 8) | CH_HORIZ;
    }

    for (int i = 1; i < height - 1; i++) {
        video_memory[(y + i) * VGA_WIDTH + x] =
            (color_local << 8) | CH_VERT;
        video_memory[(y + i) * VGA_WIDTH + x + width - 1] =
            (color_local << 8) | CH_VERT;
    }
}

void draw_title_box(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                    const char* title, uint8_t fg, uint8_t bg) {
    draw_box(x, y, width, height, fg, bg);
    if (title) {
        uint8_t title_len = strlen(title);
        if (title_len > width - 4)
            title_len = width - 4;
        uint8_t title_x = x + (width - title_len) / 2;
        for (uint8_t i = 0; i < title_len; i++) {
            video_memory[y * VGA_WIDTH + title_x + i] =
                (color << 8) | title[i];
        }
    }
}

void draw_progress_bar(uint8_t x, uint8_t y, uint8_t width, uint8_t percent,
                       uint8_t fg, uint8_t bg) {
    if (width < 2 || percent > 100) return;

    uint8_t fill = (percent * width) / 100;
    uint16_t fill_char = 219;
    uint16_t empty_char = 176;
    uint8_t bar_color = (bg << 4) | (fg & 0x0F);

    for (int i = 0; i < width; i++) {
        uint16_t c = (i < fill) ? fill_char : empty_char;
        video_memory[y * VGA_WIDTH + x + i] = (bar_color << 8) | c;
    }
}

void draw_list(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
               const char* items[], uint8_t count, uint8_t selected) {
    uint8_t fg = 15, bg = 0;
    uint8_t highlight_fg = 0, highlight_bg = 15;

    draw_box(x, y, width, height, fg, bg);

    uint8_t visible = height - 2;
    uint8_t start = 0;
    if (selected >= visible)
        start = selected - visible + 1;

    for (uint8_t i = 0; i < visible; i++) {
        uint8_t index = start + i;
        if (index >= count) break;

        uint8_t row_color = (i + start == selected)
            ? ((highlight_bg << 4) | (highlight_fg & 0x0F))
            : ((bg << 4) | (fg & 0x0F));

        const char* label = items[index];
        for (uint8_t j = 0; j < width - 2; j++) {
            char c = label[j];
            if (c == '\0') break;
            video_memory[(y + 1 + i) * VGA_WIDTH + x + 1 + j] =
                (row_color << 8) | c;
        }
    }
}

void itoa_pad(int value, char* buffer, int width) {
    char temp[16];
    int i = 0;

    if (value == 0) {
        temp[i++] = '0';
    } else {
        while (value > 0 && i < 15) {
            temp[i++] = (value % 10) + '0';
            value /= 10;
        }
    }

    while (i < width)
        temp[i++] = '0';

    for (int j = 0; j < i; j++)
        buffer[j] = temp[i - j - 1];
    buffer[i] = '\0';
}

int sputf(char* out, const char* format, ...) {
    va_list args;
    va_start(args, format);

    char* str = out;
    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%') {
            p++;
            int width = 0;
            if (*p == '0') {
                p++;
                width = *p - '0';
                p++;
            }
            if (*p == 'd') {
                int val = va_arg(args, int);
                char temp[16];
                itoa_pad(val, temp, width ? width : 1);
                for (char* t = temp; *t; t++)
                    *str++ = *t;
            }
        } else {
            *str++ = *p;
        }
    }

    *str = '\0';
    va_end(args);
    return str - out;
}

static uint16_t mouse_prev_char = 0;
static int mouse_prev_x = -1;
extern uint8_t mouse_buttons;
static int mouse_prev_y = -1;

void draw_mouse_cursor() {
    if (mouse_prev_x >= 0 && mouse_prev_y >= 0) {
        video_memory[mouse_prev_y * VGA_WIDTH + mouse_prev_x] = mouse_prev_char;
    }

    mouse_prev_char = video_memory[mouse_y * VGA_WIDTH + mouse_x];

    uint8_t fg = 15;
    uint8_t bg = 0;
    uint16_t c = CH_VERT;
    if (mouse_buttons & 1) c = 248;
    if (mouse_buttons & 2) c = CH_HORIZ;
    video_memory[mouse_y * VGA_WIDTH + mouse_x] = (bg << 4 | fg) << 8 | c;

    mouse_prev_x = mouse_x;
    mouse_prev_y = mouse_y;
}

void reset_mouse_cursor_state() {
    if (mouse_prev_x >= 0 && mouse_prev_y >= 0) {
        video_memory[mouse_prev_y * VGA_WIDTH + mouse_prev_x] = mouse_prev_char;
    }
    mouse_prev_x = -1;
    mouse_prev_y = -1;
    mouse_prev_char = (color << 8) | ' ';
}