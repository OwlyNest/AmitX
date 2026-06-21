/*
	* ui/system_manager.c - [Enter description]
	* Author:   amity
	* Date:     Tue Jun 16 23:33:37 2026
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
#define SM_LIST_X 2
#define SM_LIST_Y 2
#define SM_LIST_W 76
#define SM_LIST_H 20
/* --- Includes ---*/
#include <ui/system_manager.h>
#include <internal/kscope.h>
#include <screen/screen.h>
#include <screen/printk.h>
#include <drivers/keyboard.h>
#include <internal/amitx_consts.h>
#include <lib/string.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern uint16_t* video_memory;
extern uint8_t color;
static int sm_selected = 0;
/* --- Prototypes ---*/

/* --- Functions ---*/
// typedef enum {
//     KSCOPE_STATE_REGISTERED = 0,
//     KSCOPE_STATE_PROBING    = 1,
//     KSCOPE_STATE_OK         = 2,
//     KSCOPE_STATE_FAILED     = -1,
// } kscope_state_t;
static const char* sm_state_str(kscope_state_t state) {
    switch (state) {
		case KSCOPE_STATE_FAILED:      return "FAIL";
        case KSCOPE_STATE_REGISTERED:  return "REG";
		case KSCOPE_STATE_PROBING:     return "PROB";
        case KSCOPE_STATE_OK:          return "OK";
        default: return "ERR"; /* Undefined territory */
    }
}

static void sm_draw_list(void) {
	uint8_t fg = 15, bg = 0;
    uint8_t hi_fg = 0, hi_bg = 15;

    draw_box(SM_LIST_X, SM_LIST_Y, SM_LIST_W, SM_LIST_H, fg, bg);

    const char *title = " System Manager ";
    uint8_t tlen = strlen(title);
    uint8_t tx = SM_LIST_X + (SM_LIST_W - tlen) / 2;
    for (uint8_t i = 0; i < tlen; i++) {
        video_memory[SM_LIST_Y * VGA_WIDTH + tx + i] = (0x02 << 8) | title[i];
    }

	uint8_t visible = SM_LIST_H - 2;
    uint8_t start = 0;
    if (sm_selected >= visible) {
        start = sm_selected - visible + 1;
	}

	for (uint8_t i = 0; i < visible; i++) {
        uint8_t idx = start + i;
        if (idx >= kscope_get_count()) break;

        uint8_t row = SM_LIST_Y + 1 + i;
        uint8_t is_sel = (idx == sm_selected);
        uint8_t row_color = is_sel ? ((hi_bg << 4) | (hi_fg & 0x0F)) : ((bg << 4) | (fg & 0x0F));

        const kscope_node_t *node = kscope_get_node(idx);
        char line[76];
        ksnprintf(line, sizeof(line), "[0x%04x] (%s) %-16s %-12s", node->id, sm_state_str(node->state), node->name, kscope_class_name(node->class));

        uint8_t linelen = strlen(line);
        for (uint8_t j = 0; j < SM_LIST_W - 2 && j < linelen; j++) {
            video_memory[row * VGA_WIDTH + SM_LIST_X + 1 + j] = (row_color << 8) | line[j];
        }
        for (uint8_t j = linelen; j < SM_LIST_W - 2; j++) {
            video_memory[row * VGA_WIDTH + SM_LIST_X + 1 + j] = (row_color << 8) | ' ';
        }
    }

	const char *hint = " [\x18\x19] Select  [Enter] Details  [q] Back ";
    uint8_t hlen = strlen(hint);
    uint8_t hx = SM_LIST_X + (SM_LIST_W - hlen) / 2;
    for (uint8_t i = 0; i < hlen && hx + i < VGA_WIDTH - 1; i++) {
        video_memory[(SM_LIST_Y + SM_LIST_H - 1) * VGA_WIDTH + hx + i] = (0x01 << 8) | hint[i];
    }
}

static void sm_draw_detail(kscope_node_t *node) {
	clear();
	char title[64];
	ksnprintf(title, sizeof(title), " [0x%04x] %s ", node->id, node->name);
	draw_box(2, 1, 76, 23, 15, 0);
    uint8_t tlen = strlen(title);
    uint8_t tx = SM_LIST_X + (SM_LIST_W - tlen) / 2;
    for (uint8_t i = 0; i < tlen; i++) {
        video_memory[(SM_LIST_Y - 1) * VGA_WIDTH + tx + i] = (0x02 << 8) | title[i];
    }
	uint8_t row = 3;
    char buf[72];

	ksnprintf(buf, sizeof(buf), "  Class:     %s", kscope_class_name(node->class));
    move_cursor(4, row++); puts(buf);
	ksnprintf(buf, sizeof(buf), "  Subclass:  %s", kscope_subclass_name(node->subclass));
    move_cursor(4, row++); puts(buf);
	ksnprintf(buf, sizeof(buf), "  State:     %s", sm_state_str(node->state));
    move_cursor(4, row++); puts(buf);

	row += 1;
	move_cursor(4, row++); puts("Dependencies:\n");
	if (node->require_count != 0) {
		for (size_t i = 0; i < node->require_count; i++) {
			ksnprintf(buf, sizeof(buf), "    - %s [%s]", node->requires[i]->name, sm_state_str(node->requires[i]->state));
			move_cursor(4, row++); puts(buf);
		}
	} else {
		move_cursor(4, row++); puts("    - none");
	}
	row += 1;
	move_cursor(4, row++); puts("Capabilities:\n");
	if (node->provide_count != 0) {
		for (size_t i = 0; i < node->provide_count; i++) {
			ksnprintf(buf, sizeof(buf), "    - %s", node->provides[i]);
			move_cursor(4, row++); puts(buf);
		}
	} else {
		move_cursor(4, row++); puts("    - Why in the name of Owly does this exist?");
	}

	const char *hint = " [q] Back ";
    uint8_t hlen = strlen(hint);
    uint8_t hx = SM_LIST_X + (SM_LIST_W - hlen) / 2;
    for (uint8_t i = 0; i < hlen && hx + i < VGA_WIDTH - 1; i++) {
        video_memory[(SM_LIST_Y + SM_LIST_H + 1) * VGA_WIDTH + hx + i] = (0x01 << 8) | hint[i];
    }
}

void system_manager_run(void) {
	sm_selected = 0;
	int redraw = 1;

	while (1) {
		if (redraw) {
			clear();
			sm_draw_list();
			redraw = 0;
		}

		unsigned char c = keyboard_getchar();

		if (c == 's' || c == KEY_DOWN) {
			if (sm_selected < (int)kscope_get_count() - 1) {
				sm_selected++;
				redraw = 1;
			}
		} else if (c == 'w' || c == KEY_UP) {
			if (sm_selected > 0) {
				sm_selected--;
				redraw = 1;
			}
		} else if (c == '\n') {
			if (kscope_get_count() > 0) {
				const kscope_node_t *node = kscope_get_node(sm_selected);
				sm_draw_detail((kscope_node_t *)node);

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
		}
	}
}