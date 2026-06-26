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

//* --- Macros ---*/
#define DM_X        112
#define DM_Y        50
#define DM_W        800
#define DM_H        668
#define DM_ROW_H    20
#define DM_MAX_DEV  32

/* --- Includes ---*/
#include <ui/device_manager.h>
#include <drivers/gfx_screen.h>
#include <drivers/fb.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <hw/pci.h>
#include <screen/printk.h>
#include <lib/string.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static pci_device_t *dm_devices[DM_MAX_DEV];
static int dm_device_count = 0;
static int dm_selected = 0;

/* --- Prototypes ---*/
static void dm_refresh(void);
static void dm_draw_list(void);
static void dm_draw_detail(pci_device_t *dev);
static void dm_present(void);

/* --- Functions ---*/

/* ==========================================================================
 * Present helper
 * ======================================================================= */
static void dm_present(void) {
    fb_present();
}

/* ==========================================================================
 * Refresh device list
 * ======================================================================= */
static void dm_refresh(void) {
    dm_device_count = 0;
    pci_device_t *dev = pci_get_first_device();
    while (dev && dm_device_count < DM_MAX_DEV) {
        dm_devices[dm_device_count++] = dev;
        dev = dev->next;
    }
}

/* ==========================================================================
 * Draw list view
 * ======================================================================= */
static void dm_draw_list(void) {
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);
    uint32_t hi = gfx_theme_color(GFX_BG_HIGHLIGHT);
    uint32_t hifg = gfx_theme_color(GFX_FG_ACCENT);

    gfx_desktop();

    gfx_panel(DM_X, DM_Y, DM_W, DM_H, bg);
    gfx_bevel_in(DM_X, DM_Y, DM_W, DM_H);
    gfx_title_bar(DM_X, DM_Y, DM_W, " PCI Device Manager ");

    int list_x = DM_X + 8;
    int list_y = DM_Y + 28;
    int list_w = DM_W - 16;
    int list_h = DM_H - 60;

    int visible = list_h / DM_ROW_H;
    int start = 0;
    if (dm_selected >= visible) {
        start = dm_selected - visible + 1;
    }

    gfx_set_clip(list_x, list_y, list_w, list_h);

    for (int i = 0; i < visible && (start + i) < dm_device_count; i++) {
        int idx = start + i;
        int row_y = list_y + i * DM_ROW_H;
        uint32_t row_bg = (idx == dm_selected) ? hi : bg;
        uint32_t row_fg = (idx == dm_selected) ? hifg : fg;

        gfx_fill_rect(list_x, row_y, list_w, DM_ROW_H, row_bg);

        pci_device_t *dev = dm_devices[idx];
        char line[80];
        ksnprintf(line, sizeof(line),
                  " %02x:%02x.%x %04x:%04x %-16s %s ",
                  dev->bus, dev->device, dev->function,
                  dev->vendor_id, dev->device_id,
                  pci_subclass_name(dev->class_code, dev->subclass),
                  pci_class_name(dev->class_code));

        gfx_draw_text(list_x + 4, row_y + 6, line, row_fg);
    }

    gfx_clear_clip();

    gfx_status_bar(0, 744, 1024,
        " [Up/Down] Select  [Enter] Details  [q] Back ");

    dm_present();
}

/* ==========================================================================
 * Draw detail view
 * ======================================================================= */
static void dm_draw_detail(pci_device_t *dev) {
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);

    gfx_desktop();

    gfx_panel(DM_X, DM_Y, DM_W, DM_H, bg);
    gfx_bevel_in(DM_X, DM_Y, DM_W, DM_H);

    char title[64];
    ksnprintf(title, sizeof(title), " %02x:%02x.%x %04x:%04x ",
              dev->bus, dev->device, dev->function,
              dev->vendor_id, dev->device_id);
    gfx_title_bar(DM_X, DM_Y, DM_W, title);

    int x = DM_X + 16;
    int y = DM_Y + 36;
    char buf[80];

    ksnprintf(buf, sizeof(buf), "  Class:     %s (%s)",
              pci_class_name(dev->class_code),
              pci_subclass_name(dev->class_code, dev->subclass));
    gfx_draw_text(x, y, buf, fg); y += 16;

    ksnprintf(buf, sizeof(buf), "  Revision:  0x%02x", dev->revision);
    gfx_draw_text(x, y, buf, fg); y += 16;

    ksnprintf(buf, sizeof(buf), "  IRQ:       %u", dev->interrupt_line);
    gfx_draw_text(x, y, buf, fg); y += 16;

    ksnprintf(buf, sizeof(buf), "  Command:   0x%04x  Status: 0x%04x",
              dev->command, dev->status);
    gfx_draw_text(x, y, buf, fg); y += 24;

    gfx_draw_text(x, y, "  Base Address Registers:", fg); y += 16;

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
        gfx_draw_text(x, y, buf, fg); y += 16;
    }

    if (dev->capabilities) {
        y += 8;
        gfx_draw_text(x, y, "  Capabilities:", fg); y += 16;
        pci_capability_t *cap = dev->capabilities;
        while (cap && y < DM_Y + DM_H - 40) {
            ksnprintf(buf, sizeof(buf), "    ID 0x%02x  offset 0x%02x",
                      cap->id, cap->offset);
            gfx_draw_text(x, y, buf, fg); y += 16;
            cap = cap->next_cap;
        }
    }

    gfx_status_bar(0, 744, 1024, " [q] Back ");

    dm_present();
}

/* ==========================================================================
 * Main loop
 * ======================================================================= */
void device_manager_run(void) {
    dm_refresh();
    dm_selected = 0;
    dm_draw_list();

    while (1) {
        unsigned char c = keyboard_getchar();

        if (c == 's' || c == KEY_DOWN) {
            if (dm_selected < dm_device_count - 1) {
                dm_selected++;
                dm_draw_list();
            }
        } else if (c == 'w' || c == KEY_UP) {
            if (dm_selected > 0) {
                dm_selected--;
                dm_draw_list();
            }
        } else if (c == '\n') {
            if (dm_device_count > 0) {
                dm_draw_detail(dm_devices[dm_selected]);
                while (1) {
                    unsigned char d = keyboard_getchar();
                    if (d == 'q' || d == KEY_ESC) {
                        dm_draw_list();
                        break;
                    }
                }
            }
        } else if (c == 'q' || c == KEY_ESC) {
            break;
        }
    }
}