/*
 * phonon_consts.h - [Enter description]
 * Author:   amity
 * Date:     Tue Jun  9 19:38:56 2026
 * Copyright © 2026 OwlyNest
 */

/* --- Styling Instructions ---
 * Encoding:                      UTF-8, Unix line endings
 * Text font:                     Monospace
 * Line width:                    Max 80 characters
 * Indentation:                   Use 4 spaces
 * Brace style:                   Same line as control statement
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

#ifndef __INTERNAL_PHONON_CONSTS_H__
#define __INTERNAL_PHONON_CONSTS_H__
/* --- Includes ---*/
#include <stdint.h>
/* --- Macros ---*/
/* ==========================================================================
 * I/O Ports
 * ======================================================================= */
#define PORT_PIC_MASTER_CMD 0x20
#define PORT_PIC_MASTER_DATA 0x21
#define PORT_PIC_SLAVE_CMD 0xA0
#define PORT_PIC_SLAVE_DATA 0xA1

#define PORT_PIT_CH0 0x40
#define PORT_PIT_CH1 0x41
#define PORT_PIT_CH2 0x42
#define PORT_PIT_CMD 0x43

#define PORT_KBD_DATA 0x60
#define PORT_KBD_STATUS 0x64
#define PORT_KBD_CMD 0x64

#define PORT_VGA_CTRL 0x3D4
#define PORT_VGA_DATA 0x3D5

#define PORT_SERIAL 0x3F8

#define PORT_QEMU_EXIT 0xF4

/* ==========================================================================
 * PIC commands / configuration
 * ======================================================================= */
#define PIC_ICW1_INIT 0x11
#define PIC_ICW3_MASTER_SLAVE2 0x04
#define PIC_ICW3_SLAVE_ID2 0x02
#define PIC_ICW4_8086 0x01
#define PIC_EOI 0x20

/* ==========================================================================
 * PIT
 * ======================================================================= */
#define PIT_BASE_HZ 1193182
#define PIT_MODE_SQUARE_WAVE 0x36

/* ==========================================================================
 * Keyboard — scancode set 1
 * ======================================================================= */
#define SC_ESC 0x01
#define SC_BACKSPACE 0x0E
#define SC_TAB 0x0F
#define SC_ENTER 0x1C
#define SC_LCTRL 0x1D
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LALT 0x38
#define SC_SPACE 0x39
#define SC_CAPSLOCK 0x3A

#define SC_PREFIX_E0 0xE0
#define SC_RELEASE_BIT 0x80
#define SC_CODE_MASK 0x7F

/* Extended keys (arrive after E0 prefix) */
#define SC_E0_UP 0x48
#define SC_E0_DOWN 0x50
#define SC_E0_LEFT 0x4B
#define SC_E0_RIGHT 0x4D

/* ==========================================================================
 * Mouse (PS/2 via i8042)
 * ======================================================================= */
#define MOUSE_CMD_ENABLE_AUX 0xA8
#define MOUSE_CMD_READ_CFG 0x20
#define MOUSE_CMD_WRITE_CFG 0x60
#define MOUSE_CMD_SEND_TO_DEV 0xD4
#define MOUSE_CMD_SET_DEFAULTS 0xF6
#define MOUSE_CMD_ENABLE_STREAM 0xF4

/* ==========================================================================
 * IDT / GDT
 * ======================================================================= */
#define GDT_SEL_KERNEL_CODE 0x08
#define IDT_FLAG_PRESENT 0x80
#define IDT_TYPE_INT32 0x0E
#define IDT_FLAGS_KERNEL (IDT_FLAG_PRESENT | IDT_TYPE_INT32)
#define IDT_FLAGS_USER 0xEE

/* ==========================================================================
 * Frame constants (for PMM / paging)
 * ======================================================================= */
#define FRAME_SIZE 4096
#define FRAME_ALIGN 4096

/* ==========================================================================
 * PCI
 * ======================================================================= */
#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC
/* ==========================================================================
 * IRQ vectors after PIC remapping to 0x20
 * ======================================================================= */
#define VECTOR_IRQ0 32
#define VECTOR_IRQ1 33
#define VECTOR_IRQ4 36
#define VECTOR_IRQ10 42
#define VECTOR_IRQ12 44
#define VECTOR_SYSCALL 128

/* ==========================================================================
 * QEMU backdoor launch codes
 * ======================================================================= */
#define QEMU_APP_BASE 0x10
#define QEMU_APP_PERCH 0x11
#define QEMU_APP_OWLY 0x12

/* ==========================================================================
 * VGA text mode
 * ======================================================================= */
#define VGA_MEM_PHYS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* ==========================================================================
 * Memory layout
 * ======================================================================= */
extern uint8_t _end[];
#define HEAP_START_ADDR (((uint32_t)&_end + 0xF) & ~0xF)

/* ==========================================================================
 * Box-drawing / UI characters
 * ======================================================================= */
#define CH_UL 218     /* ┌ */
#define CH_UR 191     /* ┐ */
#define CH_LL 192     /* └ */
#define CH_LR 217     /* ┘ */
#define CH_HORIZ 196  /* ─ */
#define CH_VERT 179   /* │ */
#define CH_FILL 219   /* █ */
#define CH_EMPTY 176  /* ░ */
#define CH_CURSOR 179 /* │ */

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
#endif