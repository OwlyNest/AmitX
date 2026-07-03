/*
	* ui/device_manager.c - PCI Device Manager (windowed)
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
#define DM_W        640
#define DM_H        480
#define DM_ROW_H    20
#define DM_MAX_DEV  32

/* --- Includes ---*/
#include <ui/device_manager.h>
#include <gfx/window.h>
#include <gfx/compositor.h>
#include <gfx/fb.h>
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

window_handle_t dm;
static window_t *win;

/* --- Prototypes ---*/
static void dm_refresh(void);
static void dm_draw_list(void);
static void dm_draw_detail(pci_device_t *dev);

/* --- Functions ---*/

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

    gfx_clear(&win->surface, gfx_theme_color(GFX_BG_DESKTOP));

    int panel_x = 8;
    int panel_y = 8;
    int panel_w = DM_W - 16;
    int panel_h = DM_H - 44;

    gfx_panel(&win->surface, panel_x, panel_y, panel_w, panel_h, bg);
    gfx_bevel_in(&win->surface, panel_x, panel_y, panel_w, panel_h);
    gfx_title_bar(&win->surface, panel_x, panel_y, panel_w, " PCI Device Manager ");

    int list_x = panel_x + 8;
    int list_y = panel_y + 28;
    int list_w = panel_w - 16;
    int list_h = panel_h - 40;

    int visible = list_h / DM_ROW_H;
    int start = 0;
    if (dm_selected >= visible) {
        start = dm_selected - visible + 1;
    }

    for (int i = 0; i < visible && (start + i) < dm_device_count; i++) {
        int idx = start + i;
        int row_y = list_y + i * DM_ROW_H;
        uint32_t row_bg = (idx == dm_selected) ? hi : bg;
        uint32_t row_fg = (idx == dm_selected) ? hifg : fg;

        gfx_fill_rect(&win->surface, list_x, row_y, list_w, DM_ROW_H, row_bg);

        pci_device_t *dev = dm_devices[idx];
        char line[80];
        ksnprintf(line, sizeof(line),
                  " %02x:%02x.%x %04x:%04x %-16s %s ",
                  dev->bus, dev->device, dev->function,
                  dev->vendor_id, dev->device_id,
                  pci_subclass_name(dev->class_code, dev->subclass),
                  pci_class_name(dev->class_code));

        window_draw_text(dm, list_x + 4, row_y + 6, line, row_fg);
    }

    gfx_status_bar(&win->surface, 0, DM_H - 28, DM_W,
        " [Up/Down] Select  [Enter] Details  [q] Back ");
}

/* ==========================================================================
 * Draw detail view
 * ======================================================================= */
static void dm_draw_detail(pci_device_t *dev) {
    uint32_t fg = gfx_theme_color(GFX_FG_TEXT);
    uint32_t bg = gfx_theme_color(GFX_BG_PANEL);

    gfx_clear(&win->surface, gfx_theme_color(GFX_BG_DESKTOP));

    int panel_x = 8;
    int panel_y = 8;
    int panel_w = DM_W - 16;
    int panel_h = DM_H - 44;

    gfx_panel(&win->surface, panel_x, panel_y, panel_w, panel_h, bg);
    gfx_bevel_in(&win->surface, panel_x, panel_y, panel_w, panel_h);

    char title[64];
    ksnprintf(title, sizeof(title), " %02x:%02x.%x %04x:%04x ", dev->bus, dev->device, dev->function, dev->vendor_id, dev->device_id);
    gfx_title_bar(&win->surface, panel_x, panel_y, panel_w, title);

    int x = panel_x + 16;
    int y = panel_y + 36;
    char buf[80];

    ksnprintf(buf, sizeof(buf), "  Class:     %s (%s)", pci_class_name(dev->class_code), pci_subclass_name(dev->class_code, dev->subclass));
    window_draw_text(dm, x, y, buf, fg); y += 16;

    ksnprintf(buf, sizeof(buf), "  Revision:  0x%02x", dev->revision);
    window_draw_text(dm, x, y, buf, fg); y += 16;

    ksnprintf(buf, sizeof(buf), "  IRQ:       %u", dev->interrupt_line);
    window_draw_text(dm, x, y, buf, fg); y += 16;

    ksnprintf(buf, sizeof(buf), "  Command:   0x%04x  Status: 0x%04x", dev->command, dev->status);
    window_draw_text(dm, x, y, buf, fg); y += 24;

    window_draw_text(dm, x, y, "  Base Address Registers:", fg); y += 16;

    for (uint8_t i = 0; i < dev->bar_count; i++) {
        if (dev->bars[i].base == 0 && dev->bars[i].size == 0)
            continue;

        if (dev->bars[i].is_io) {
            ksnprintf(buf, sizeof(buf), "    BAR%d  I/O  0x%08x  size 0x%x", i, dev->bars[i].base, dev->bars[i].size);
        } else {
            ksnprintf(buf, sizeof(buf), "    BAR%d  MEM  0x%08x  size 0x%x  %s", i, dev->bars[i].base, dev->bars[i].size, dev->bars[i].is_prefetch ? "prefetchable" : "");
        }
        window_draw_text(dm, x, y, buf, fg); y += 16;
    }

    if (dev->capabilities) {
        y += 8;
        window_draw_text(dm, x, y, "  Capabilities:", fg); y += 16;
        pci_capability_t *cap = dev->capabilities;
        while (cap && y < panel_y + panel_h - 40) {
            ksnprintf(buf, sizeof(buf), "    ID 0x%02x  offset 0x%02x", cap->id, cap->offset);
            window_draw_text(dm, x, y, buf, fg); y += 16;
            cap = cap->next_cap;
        }
    }

    gfx_status_bar(&win->surface, 0, DM_H - 28, DM_W, " [q] Back ");
}

/* ==========================================================================
 * Main loop
 * ======================================================================= */
void device_manager_run(void) {
    int win_x = (fb.back.width - DM_W) / 2;
    int win_y = (fb.back.height - DM_H) / 2;

    dm = window_create(win_x, win_y, DM_W, DM_H, "PCI Device Manager", WIN_FLAG_TITLEBAR);
    if ((int)dm == WIN_INVALID) {
        printk("[dm] Failed to create window\n");
        return;
    }

    win = window_get(dm);
    if (!win) {
        printk("[dm] Failed to get window\n");
        return;
    }

    dm_refresh();
    dm_selected = 0;
    dm_draw_list();
    compositor_render();

    while (1) {
        unsigned char c = keyboard_getchar();

        if (c == 's' || c == KEY_DOWN) {
            if (dm_selected < dm_device_count - 1) {
                dm_selected++;
                dm_draw_list();
                compositor_render();
            }
        } else if (c == 'w' || c == KEY_UP) {
            if (dm_selected > 0) {
                dm_selected--;
                dm_draw_list();
                compositor_render();
            }
        } else if (c == '\n') {
            if (dm_device_count > 0) {
                dm_draw_detail(dm_devices[dm_selected]);
                compositor_render();

                while (1) {
                    unsigned char d = keyboard_getchar();
                    if (d == 'q' || d == KEY_ESC) {
                        dm_draw_list();
                        compositor_render();
                        break;
                    }
                }
            }
        } else if (c == 'q' || c == KEY_ESC) {
            break;
        }
    }

    window_destroy(dm);
}