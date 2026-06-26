/*
	* gfx/svga.c - VMware SVGA-II display driver
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

/* --- Includes ---*/
#include <gfx/svga.h>
#include <hw/pci.h>
#include <arch/x86/io.h>
#include <screen/printk.h>
#include <mm/paging.h>
#include <stdint.h>

/* --- Globals ---*/
svga_device_t svga = { 0 };

/* --- Functions ---*/
/* ==========================================================================
 * Register access
 * ======================================================================= */
uint32_t svga_read_reg(uint32_t index) {
    outl(svga.io_base + SVGA_OFF_INDEX, index);
    return inl(svga.io_base + SVGA_OFF_VALUE);
}

void svga_write_reg(uint32_t index, uint32_t value) {
    outl(svga.io_base + SVGA_OFF_INDEX, index);
    outl(svga.io_base + SVGA_OFF_VALUE, value);
}

/* ==========================================================================
 * PCI discovery and BAR assignment
 *
 * SVGA-II BARs are always in fixed order:
 *   BAR0 = I/O ports, BAR1 = framebuffer, BAR2 = FIFO.
 * Do NOT assign by size — VirtualBox gives both memory BARs the same
 * size (0x200000) which defeats any size-based heuristic.
 * ======================================================================= */
static int svga_find_device(void) {
    pci_device_t *dev = pci_get_device(
        SVGA_PCI_VENDOR_VMWARE, SVGA_PCI_DEVICE_SVGA2);
    if (!dev) {
        printk("[svga] Device not found\n");
        return -1;
    }

    printk("[svga] Found at %02x:%02x.%x\n",
           dev->bus, dev->device, dev->function);

    /* Enable I/O, MMIO, and bus mastering */
    uint32_t cmd = pci_read_config(
        dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd |= 0x07;
    pci_write_config(
        dev->bus, dev->device, dev->function, PCI_COMMAND, cmd);

    for (uint8_t i = 0; i < dev->bar_count && i < 3; i++) {
        if (dev->bars[i].base == 0) continue;

        if (dev->bars[i].is_io) {
            /* BAR0: I/O ports */
            svga.io_base = (uint16_t)dev->bars[i].base;
            printk("[svga] BAR%d I/O  0x%04x\n",
                   i, svga.io_base);
        } else if (svga.fb_phys == 0) {
            /* BAR1: framebuffer (first memory BAR) */
            svga.fb_phys = dev->bars[i].base;
            printk("[svga] BAR%d FB   0x%08x (%u KB)\n",
                   i, svga.fb_phys, dev->bars[i].size / 1024);
        } else {
            /* BAR2: FIFO (second memory BAR) */
            svga.fifo_phys = dev->bars[i].base;
            printk("[svga] BAR%d FIFO 0x%08x (%u KB)\n",
                   i, svga.fifo_phys, dev->bars[i].size / 1024);
        }
    }

    if (!svga.io_base || !svga.fb_phys || !svga.fifo_phys) {
        printk("[svga] BAR setup incomplete "
               "(io=0x%04x fb=0x%08x fifo=0x%08x)\n",
               svga.io_base, svga.fb_phys, svga.fifo_phys);
        return -1;
    }
    return 0;
}

/* ==========================================================================
 * Framebuffer mapping
 *
 * The visible surface starts at fb_phys + FB_OFFSET, not necessarily
 * at fb_phys itself.  Always add the offset.
 * ======================================================================= */
static int svga_map_fb(void) {
    uint32_t fb_offset = svga_read_reg(SVGA_REG_FB_OFFSET);
    uint32_t fb_phys   = svga.fb_phys + fb_offset;

    /*
     * Map only the visible area for the current mode.
     *
     * SVGA_REG_FB_SIZE reports total VRAM (e.g. 16 MB on QEMU), NOT the
     * size needed for the current resolution.  Mapping all of it from
     * SVGA_FB_VIRT can overflow into the recursive-mapping region
     * (0xFFC00000-0xFFFFFFFF), overwriting PD[1023] and destroying the
     * page directory's self-reference, which triple-faults the kernel.
     *
     * Only map pitch * height — the exact bytes the current mode uses.
     */
    uint32_t fb_size = svga.pitch * svga.height;
    if (fb_size == 0) {
        printk("[svga] Cannot determine FB size (mode not set?)\n");
        return -1;
    }

    /*
     * Hard safety check: the mapping must not reach the recursive region.
     * 0xFFC00000 is where paging.c maps page tables via PD[1023].
     * If this fires, lower SVGA_FB_VIRT or reduce the resolution.
     */
    if (SVGA_FB_VIRT + fb_size > 0xFFC00000u) {
        printk("[svga] FB mapping 0x%08x+%u would hit recursive "
               "region — aborting\n", SVGA_FB_VIRT, fb_size);
        return -1;
    }

    uint32_t pages = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t phys = fb_phys        + i * PAGE_SIZE;
        uint32_t virt = SVGA_FB_VIRT   + i * PAGE_SIZE;
        if (map_page(phys, virt, PAGE_WRITABLE | PAGE_NOCACHE) != 0) {
            printk("[svga] FB map failed page %u "
                   "(phys 0x%08x)\n", i, phys);
            return -1;
        }
    }

    svga.fb_virt = (void *)SVGA_FB_VIRT;
    printk("[svga] FB  0x%08x+0x%05x -> 0x%08x (%u KB)\n",
           svga.fb_phys, fb_offset, SVGA_FB_VIRT, fb_size / 1024);
    return 0;
}

/* ==========================================================================
 * FIFO initialisation
 *
 * Map the FIFO BAR, then program the four mandatory header registers
 * before telling the device CONFIG_DONE.  The header occupies the
 * first SVGA_FIFO_NUM_REGS dwords; commands start right after.
 * ======================================================================= */
static int svga_fifo_init(void) {
    uint32_t fifo_size = svga_read_reg(SVGA_REG_FIFO_SIZE);
    if (fifo_size == 0) {
        printk("[svga] No FIFO\n");        /* device is not SVGA-II    */
        return -1;
    }

    uint32_t pages = (fifo_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t phys = svga.fifo_phys   + i * PAGE_SIZE;
        uint32_t virt = SVGA_FIFO_VIRT + i * PAGE_SIZE;
        if (map_page(phys, virt, PAGE_WRITABLE) != 0) {
            printk("[svga] FIFO map failed page %u\n", i);
            return -1;
        }
    }
    svga.fifo_virt = (void *)SVGA_FIFO_VIRT;

    /* Initialise the four mandatory FIFO header registers */
    volatile uint32_t *fifo   = (volatile uint32_t *)svga.fifo_virt;
    uint32_t           cmd_start =                   /* first cmd byte offset */
        SVGA_FIFO_NUM_REGS * sizeof(uint32_t);

    fifo[SVGA_FIFO_MIN]      = cmd_start;
    fifo[SVGA_FIFO_MAX]      = fifo_size;
    fifo[SVGA_FIFO_NEXT_CMD] = cmd_start;
    fifo[SVGA_FIFO_STOP]     = cmd_start;

    svga_write_reg(SVGA_REG_CONFIG_DONE, 1);
    printk("[svga] FIFO 0x%08x -> 0x%08x (%u KB)\n",
           svga.fifo_phys, SVGA_FIFO_VIRT, fifo_size / 1024);
    return 0;
}

/* ==========================================================================
 * FIFO write and sync
 * ======================================================================= */
void svga_fifo_write(uint32_t value) {
    if (!svga.fifo_virt) return;

    volatile uint32_t *fifo = (volatile uint32_t *)svga.fifo_virt;
    uint32_t           min  = fifo[SVGA_FIFO_MIN];
    uint32_t           max  = fifo[SVGA_FIFO_MAX];
    uint32_t           next = fifo[SVGA_FIFO_NEXT_CMD];

    uint32_t next_next = next + sizeof(uint32_t);
    if (next_next >= max) next_next = min;

    /* Stall until there is room */
    while (next_next == fifo[SVGA_FIFO_STOP]) {
        svga_write_reg(SVGA_REG_SYNC, 1);
        __asm__ volatile("pause");
    }

    *(volatile uint32_t *)((uintptr_t)svga.fifo_virt + next) = value;
    __asm__ volatile("" ::: "memory");         /* compiler barrier     */
    fifo[SVGA_FIFO_NEXT_CMD] = next_next;
}

static void svga_sync(void) {
    svga_write_reg(SVGA_REG_SYNC, 1);
    while (svga_read_reg(SVGA_REG_BUSY))
        __asm__ volatile("pause");
}

/* ==========================================================================
 * Update commands
 * ======================================================================= */
void svga_update_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    svga_fifo_write(SVGA_CMD_UPDATE);
    svga_fifo_write(x);
    svga_fifo_write(y);
    svga_fifo_write(w);
    svga_fifo_write(h);
    svga_sync();
}

void svga_update_full(void) {
    if (!svga.initialized) return;
    svga_update_rect(0, 0, svga.width, svga.height);
}

/* ==========================================================================
 * Mode set
 *
 * Sets resolution registers and re-maps the framebuffer.
 * Can be called after svga_init() to change modes.
 * ======================================================================= */
int svga_set_resolution(uint32_t width, uint32_t height, uint32_t bpp) {
    svga_write_reg(SVGA_REG_ENABLE, 0);         /* disable before change    */
    svga_write_reg(SVGA_REG_WIDTH,  width);
    svga_write_reg(SVGA_REG_HEIGHT, height);
    svga_write_reg(SVGA_REG_BPP,    bpp);
    svga_write_reg(SVGA_REG_ENABLE, 1);

    svga.width  = svga_read_reg(SVGA_REG_WIDTH);
    svga.height = svga_read_reg(SVGA_REG_HEIGHT);
    svga.bpp    = svga_read_reg(SVGA_REG_BPP);
    svga.pitch  = svga_read_reg(SVGA_REG_BYTES_PER_LINE);

    printk("[svga] %ux%u @ %ubpp pitch=%u\n",
           svga.width, svga.height, svga.bpp, svga.pitch);

    if (svga.width == 0 || svga.height == 0) {
        printk("[svga] Resolution rejected by device\n");
        return -1;
    }
    return 0;
}

/* ==========================================================================
 * Full initialisation sequence
 *
 * Call once at boot.  Do NOT call svga_set_resolution() again from
 * kernel_main — it has already been called here.
 * ======================================================================= */
int svga_init(void) {
    if (svga_find_device() != 0) return -1;

    /* Negotiate the highest supported protocol version */
    svga_write_reg(SVGA_REG_ID, SVGA_ID_2);
    svga.version = svga_read_reg(SVGA_REG_ID);

    if (svga.version < SVGA_ID_0) {
        printk("[svga] Unsupported version 0x%08x\n", svga.version);
        return -1;
    }
    printk("[svga] Protocol 0x%08x\n", svga.version);

    svga.vram_size = svga_read_reg(SVGA_REG_VRAM_SIZE);

    /* Set mode, map FB, init FIFO — in that order */
    if (svga_set_resolution(1024, 768, 32) != 0) return -1;
    if (svga_map_fb()                      != 0) return -1;
    if (svga_fifo_init()                   != 0) return -1;

    svga.initialized = 1;
    printk("[svga] Ready %ux%u VRAM %u KB\n",
           svga.width, svga.height, svga.vram_size / 1024);
    return 0;
}