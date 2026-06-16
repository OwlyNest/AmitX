/*
	* ui/device_manager.c - [Enter description]
	* Author:   amity
	* Date:     Wed Jun 17 01:17:54 2026
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
#define DM_LIST_X       1
#define DM_LIST_Y       2
#define DM_LIST_W       78
#define DM_LIST_H       20
#define DM_MAX_DEVICES  32

/* --- Includes ---*/
#include <ui/device_manager.h>
#include <screen/screen.h>
#include <screen/printk.h>
#include <drivers/keyboard.h>
#include <hw/pci.h>
#include <internal/amitx_consts.h>
#include <lib/string.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static pci_device_t *dm_devices[DM_MAX_DEVICES];
static int dm_device_count = 0;
static int dm_selected = 0;
extern uint16_t* video_memory;
extern uint8_t color;
/* --- Prototypes ---*/

/* --- Functions ---*/
static void dm_refresh_devices(void) {
    dm_device_count = 0;
    pci_device_t *dev = pci_get_first_device();
    while (dev && dm_device_count < DM_MAX_DEVICES) {
        dm_devices[dm_device_count++] = dev;
        dev = dev->next;
    }
}

static void dm_draw_list(void) {
    uint8_t fg = 15, bg = 0;
    uint8_t hi_fg = 0, hi_bg = 15;

    draw_box(DM_LIST_X, DM_LIST_Y, DM_LIST_W, DM_LIST_H, fg, bg);

    const char *title = " PCI Device Manager ";
    uint8_t tlen = strlen(title);
    uint8_t tx = DM_LIST_X + (DM_LIST_W - tlen) / 2;
    for (uint8_t i = 0; i < tlen; i++) {
        video_memory[DM_LIST_Y * VGA_WIDTH + tx + i] = (0x02 << 8) | title[i];
    }

    uint8_t visible = DM_LIST_H - 2;
    uint8_t start = 0;
    if (dm_selected >= visible) {
        start = dm_selected - visible + 1;
    }

    for (uint8_t i = 0; i < visible; i++) {
        uint8_t idx = start + i;
        if (idx >= dm_device_count) break;

        uint8_t row = DM_LIST_Y + 1 + i;
        uint8_t is_sel = (idx == dm_selected);
        uint8_t row_color = is_sel
            ? ((hi_bg << 4) | (hi_fg & 0x0F))
            : ((bg << 4) | (fg & 0x0F));

        pci_device_t *dev = dm_devices[idx];
        char line[76];
        ksnprintf(line, sizeof(line),
            " %02x:%02x.%x %04x:%04x %-16s %s ",
            dev->bus, dev->device, dev->function,
            dev->vendor_id, dev->device_id,
            pci_subclass_name(dev->class_code, dev->subclass),
            pci_class_name(dev->class_code));

        uint8_t linelen = strlen(line);
        for (uint8_t j = 0; j < DM_LIST_W - 2 && j < linelen; j++) {
            video_memory[row * VGA_WIDTH + DM_LIST_X + 1 + j] =
                (row_color << 8) | line[j];
        }
        for (uint8_t j = linelen; j < DM_LIST_W - 2; j++) {
            video_memory[row * VGA_WIDTH + DM_LIST_X + 1 + j] =
                (row_color << 8) | ' ';
        }
    }

    const char *hint = " [\x18\x19] Select  [Enter] Details  [q] Back ";
    uint8_t hlen = strlen(hint);
    uint8_t hx = DM_LIST_X + (DM_LIST_W - hlen) / 2;
    for (uint8_t i = 0; i < hlen && hx + i < VGA_WIDTH - 1; i++) {
        video_memory[(DM_LIST_Y + DM_LIST_H - 1) * VGA_WIDTH + hx + i] =
            (0x01 << 8) | hint[i];
    }
}

static void dm_draw_detail(pci_device_t *dev) {
    clear();

    char title[64];
    ksnprintf(title, sizeof(title), " %02x:%02x.%x %04x:%04x ",
              dev->bus, dev->device, dev->function,
              dev->vendor_id, dev->device_id);

    draw_box(2, 1, 76, 23, 15, 0);
    uint8_t tlen = strlen(title);
    uint8_t tx = DM_LIST_X + (DM_LIST_W - tlen) / 2;
    for (uint8_t i = 0; i < tlen; i++) {
        video_memory[(DM_LIST_Y - 1) * VGA_WIDTH + tx + i] = (0x02 << 8) | title[i];
    }

    uint8_t row = 3;
    char buf[72];

    ksnprintf(buf, sizeof(buf), "  Class:     %s (%s)",
              pci_class_name(dev->class_code),
              pci_subclass_name(dev->class_code, dev->subclass));
    move_cursor(4, row++); puts(buf);

    ksnprintf(buf, sizeof(buf), "  Revision:  0x%02x", dev->revision);
    move_cursor(4, row++); puts(buf);

    ksnprintf(buf, sizeof(buf), "  IRQ:       %u", dev->interrupt_line);
    move_cursor(4, row++); puts(buf);

    ksnprintf(buf, sizeof(buf), "  Command:   0x%04x  Status: 0x%04x",
              dev->command, dev->status);
    move_cursor(4, row++); puts(buf);

    row += 1;
    move_cursor(4, row++); puts("  Base Address Registers:");

    for (uint8_t i = 0; i < dev->bar_count; i++) {
        if (dev->bars[i].base == 0 && dev->bars[i].size == 0)
            continue;

        if (dev->bars[i].is_io) {
            ksnprintf(buf, sizeof(buf),
                "    BAR%d  I/O  0x%08x  size 0x%x",
                i, dev->bars[i].base, dev->bars[i].size);
        } else {
            ksnprintf(buf, sizeof(buf),
                "    BAR%d  MEM  0x%08x  size 0x%x  %s",
                i, dev->bars[i].base, dev->bars[i].size,
                dev->bars[i].is_prefetch ? "prefetchable" : "");
        }
        move_cursor(4, row++); puts(buf);
    }

    if (dev->capabilities) {
        row += 1;
        move_cursor(4, row++); puts("  Capabilities:");
        pci_capability_t *cap = dev->capabilities;
        while (cap && row < 22) {
            ksnprintf(buf, sizeof(buf), "    ID 0x%02x  offset 0x%02x",
                      cap->id, cap->offset);
            move_cursor(4, row++); puts(buf);
            cap = cap->next_cap;
        }
    }

    const char *hint = " [q] Back ";
    uint8_t hlen = strlen(hint);
    uint8_t hx = DM_LIST_X + (DM_LIST_W - hlen) / 2;
    for (uint8_t i = 0; i < hlen && hx + i < VGA_WIDTH - 1; i++) {
        video_memory[(DM_LIST_Y + DM_LIST_H + 1) * VGA_WIDTH + hx + i] =
            (0x01 << 8) | hint[i];
    }
}

void device_manager_run(void) {
    dm_refresh_devices();
    dm_selected = 0;
    int redraw = 1;

    while (1) {
        if (redraw) {
            clear();
            dm_draw_list();
            redraw = 0;
        }

        unsigned char c = keyboard_getchar();

        if (c == 's' || c == KEY_DOWN) {
            if (dm_selected < dm_device_count - 1) {
                dm_selected++;
                redraw = 1;
            }
        } else if (c == 'w' || c == KEY_UP) {
            if (dm_selected > 0) {
                dm_selected--;
                redraw = 1;
            }
        } else if (c == '\n') {
            if (dm_device_count > 0) {
                dm_draw_detail(dm_devices[dm_selected]);
                while (1) {
                    unsigned char d = keyboard_getchar();
                    if (d == 'q' || d == KEY_ESC) {
                        redraw = 1;
                        break;
                    }
                }
            }
        } else if (c == 'q' || c == KEY_ESC) {
            break;
        } else {
            redraw = 1;
        }
    }
}