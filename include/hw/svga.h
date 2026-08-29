/*
	* include/drivers/svga.h - VMware SVGA-II display driver
	* Author:   amity
	* Date:     Wed Jun 24 00:00:00 2026
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
#ifndef __HW_SVGA_H__
#define __HW_SVGA_H__

/* --- PCI identity ---*/
#define SVGA_PCI_VENDOR_VMWARE      0x15AD
#define SVGA_PCI_DEVICE_SVGA2       0x0405

/*
 * SVGA-II always exposes exactly three BARs in this order:
 *   BAR0  I/O ports  (index + value registers)
 *   BAR1  MMIO       framebuffer
 *   BAR2  MMIO       FIFO command buffer
 * Never rely on BAR size to tell FB from FIFO: VirtualBox gives
 * both as 0x200000 which breaks a size-based heuristic.
 */

/* --- I/O port offsets (byte offsets from io_base) ---*/
#define SVGA_OFF_INDEX              0       /* 32-bit index register    */
#define SVGA_OFF_VALUE              1       /* 32-bit value register    */

/* --- Register indices ---*/
#define SVGA_REG_ID                 0
#define SVGA_REG_ENABLE             1
#define SVGA_REG_WIDTH              2
#define SVGA_REG_HEIGHT             3
#define SVGA_REG_MAX_WIDTH          4
#define SVGA_REG_MAX_HEIGHT         5
#define SVGA_REG_DEPTH              6
#define SVGA_REG_BPP                7
#define SVGA_REG_PSEUDOCOLOR        8
#define SVGA_REG_RED_MASK           9
#define SVGA_REG_GREEN_MASK         10
#define SVGA_REG_BLUE_MASK          11
#define SVGA_REG_BYTES_PER_LINE     12
#define SVGA_REG_FB_START           13
#define SVGA_REG_FB_OFFSET          14      /* byte offset within BAR1  */
#define SVGA_REG_VRAM_SIZE          15
#define SVGA_REG_FB_SIZE            16
#define SVGA_REG_CAPABILITIES       17
#define SVGA_REG_FIFO_START         18      /* physical FIFO base       */
#define SVGA_REG_FIFO_SIZE          19      /* FIFO buffer size (bytes) */
#define SVGA_REG_CONFIG_DONE        20
#define SVGA_REG_SYNC               21
#define SVGA_REG_BUSY               22

/* --- Protocol IDs ---*/
#define SVGA_ID_0                   0x90000000
#define SVGA_ID_1                   0x90000001
#define SVGA_ID_2                   0x90000002

/* --- FIFO commands ---*/
#define SVGA_CMD_UPDATE             1

/* --- FIFO header register slots (indices into the FIFO dword array) ---*/
#define SVGA_FIFO_MIN               0       /* byte offset of cmd area  */
#define SVGA_FIFO_MAX               1       /* byte offset of FIFO end  */
#define SVGA_FIFO_NEXT_CMD          2       /* next write offset        */
#define SVGA_FIFO_STOP              3       /* last read offset         */
#define SVGA_FIFO_NUM_REGS          4       /* header occupies 4 dwords */

/*
 * Virtual address windows.
 *
 * FB  : 0xFF000000 – 0xFF7FFFFF  (8 MB, enough for 1920x1200x32)
 * FIFO: 0xFF800000 – 0xFF9FFFFF  (2 MB, more than any device needs)
 *
 * There is a 0-byte gap between them which is fine because the
 * FB window ends exactly where the FIFO window starts.
 */
#define SVGA_FB_VIRT                0xFE000000u
#define SVGA_FIFO_VIRT              0xFE800000u

/* --- Includes ---*/
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/
typedef struct svga_device {
    uint16_t  io_base;          /* I/O port base (BAR0)             */
    uintptr_t fb_phys;          /* Framebuffer BAR physical base    */
    uintptr_t fifo_phys;        /* FIFO BAR physical base           */
    void     *fb_virt;          /* Mapped framebuffer virtual addr  */
    void     *fifo_virt;        /* Mapped FIFO virtual addr         */
    uint32_t  width;
    uint32_t  height;
	uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t  bpp;
    uint32_t  pitch;            /* bytes per scanline               */
    uint32_t  vram_size;
    uint32_t  version;
    int       initialized;
} svga_device_t;

/* --- Globals ---*/
extern svga_device_t svga;

/* --- Prototypes ---*/
uint32_t svga_read_reg(uint32_t index);
void     svga_write_reg(uint32_t index, uint32_t value);
int      svga_init(void);
int      svga_set_resolution(uint32_t width, uint32_t height, uint32_t bpp);
void     svga_fifo_write(uint32_t value);
void     svga_update_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void     svga_update_full(void);

#endif