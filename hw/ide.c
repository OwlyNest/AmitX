/*
	* hw/ide.c - ATA/ATAPI IDE controller driver (PIO mode)
	* Author:   amity
	* Date:     Thu Jun 11 15:08:10 2026
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
#include <hw/ide.h>
#include <arch/x86/io.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static uint16_t ide_data_base = 0;
static uint16_t ide_ctrl_base = 0;

/* --- Prototypes ---*/
static void ide_delay_400ns(void);
static int ide_wait_bsy(void);
static int ide_poll_ready(void);
static void ide_report_error(const char *op);
static int ide_select_drive(uint8_t drive);

/* --- Functions ---*/

/* ==========================================================================
 * ATA spec requires 400ns delay after drive select.
 * Reading alternate status 4 times achieves this reliably
 * regardless of CPU frequency.
 * ======================================================================= */
static void ide_delay_400ns(void) {
    inb(ide_ctrl_base + IDE_ALT_STATUS);
    inb(ide_ctrl_base + IDE_ALT_STATUS);
    inb(ide_ctrl_base + IDE_ALT_STATUS);
    inb(ide_ctrl_base + IDE_ALT_STATUS);
}

/* ==========================================================================
 * Wait for BSY to clear, with timeout.
 * Returns 1 on success, 0 on timeout.
 * ======================================================================= */
static int ide_wait_bsy(void) {
    for (uint32_t timeout = 0; timeout < 1000000; timeout++) {
        uint8_t status = inb(ide_data_base + IDE_STATUS);
        if (!(status & IDE_STATUS_BSY)) {
            return 1;
        }
    }
    return 0;
}

/* ==========================================================================
 * Poll after a command: wait for BSY clear, then check ERR/DF/DRQ.
 *
 * Per ATA-8:
 *   1. Wait for BSY = 0
 *   2. Check ERR or DF. If set -> failure.
 *   3. Check DRQ. If set -> ready to transfer.
 *   4. If RDY = 1 and DRQ = 0 -> command done, no data.
 *
 * Returns:  1  DRQ set, ready to transfer
 *           0  Error, timeout, or command complete without data
 * ======================================================================= */
static int ide_poll_ready(void) {
    for (uint32_t timeout = 0; timeout < 1000000; timeout++) {
        uint8_t status = inb(ide_data_base + IDE_STATUS);

        if (status & IDE_STATUS_BSY) {
            continue;
        }

        /* BSY clear: check for errors */
        if (status & (IDE_STATUS_ERR | IDE_STATUS_DF)) {
            return 0;
        }

        if (status & IDE_STATUS_DRQ) {
            return 1;
        }

        /* RDY without DRQ = command complete */
        if (status & IDE_STATUS_RDY) {
            return 0;
        }
    }
    return 0;
}

/* ==========================================================================
 * Read and report error register contents
 * ======================================================================= */
static void ide_report_error(const char *op) {
    uint8_t err = inb(ide_data_base + IDE_ERROR);

    printk("[ide] %s error: ERR=0x%02x (", op, err);
    if (err & 0x01) printk("AMNF ");
    if (err & 0x02) printk("TK0NF ");
    if (err & 0x04) printk("ABRT ");
    if (err & 0x08) printk("MCR ");
    if (err & 0x10) printk("IDNF ");
    if (err & 0x20) printk("MC ");
    if (err & 0x40) printk("UNC ");
    if (err & 0x80) printk("BBK ");
    printk(")\n");
}

/* ==========================================================================
 * Select drive and wait for it to settle.
 * Returns 0 on success, -1 if no drive present.
 * ======================================================================= */
static int ide_select_drive(uint8_t drive) {
    if (drive > 1) {
        return -1;
    }

    outb(ide_data_base + IDE_DRIVE_SELECT, 0xA0 | (drive << 4));
    ide_delay_400ns();

    /* If status is 0x00 or 0xFF, nothing is connected */
    uint8_t status = inb(ide_data_base + IDE_STATUS);
    if (status == 0x00 || status == 0xFF) {
        return -1;
    }
    return 0;
}

/* ==========================================================================
 * Initialize IDE channel
 * ======================================================================= */
void ide_init(uint16_t data_base, uint16_t ctrl_base) {
    ide_data_base = data_base;
    ide_ctrl_base = ctrl_base;

    printk("[ide] Primary channel at I/O 0x%04x, control at 0x%04x\n", data_base, ctrl_base);

    /* Software reset: assert SRST + nIEN, wait, de-assert SRST.
     * nIEN stays asserted because we are polling, not using IRQs. */
    outb(ide_ctrl_base, 0x04 | 0x02);
    ide_delay_400ns();
    outb(ide_ctrl_base, 0x02);
    ide_delay_400ns();

    /* Wait for BSY to clear after reset */
    if (!ide_wait_bsy()) {
        printk("[ide] Warning: BSY stuck after reset\n");
    }
}

/* ==========================================================================
 * IDENTIFY DEVICE
 * ======================================================================= */
int ide_identify(uint8_t drive, uint16_t *buf) {
    if (!buf)
        return -1;

    if (ide_select_drive(drive) != 0) {
        printk("[ide] Drive %d not present\n", drive);
        return -1;
    }

    /* Send IDENTIFY parameters (all zeros) */
    outb(ide_data_base + IDE_SECTOR_COUNT, 0);
    outb(ide_data_base + IDE_LBA_LOW, 0);
    outb(ide_data_base + IDE_LBA_MID, 0);
    outb(ide_data_base + IDE_LBA_HIGH, 0);

    outb(ide_data_base + IDE_COMMAND, IDE_CMD_IDENTIFY);

    if (!ide_wait_bsy()) {
        printk("[ide] Drive %d: BSY timeout after IDENTIFY\n", drive);
        return -1;
    }

    /* Check for non-ATA signatures */
    uint8_t mid  = inb(ide_data_base + IDE_LBA_MID);
    uint8_t high = inb(ide_data_base + IDE_LBA_HIGH);

    if (mid == 0x14 && high == 0xEB) {
        printk("[ide] Drive %d: ATAPI device (not supported)\n", drive);
        return -1;
    }
    if (mid == 0x3C && high == 0xC3) {
        printk("[ide] Drive %d: SATA device (not supported)\n", drive);
        return -1;
    }
    if (mid != 0 || high != 0) {
        printk("[ide] Drive %d: Unknown signature 0x%02x%02x\n", drive, high, mid);
        return -1;
    }

    if (!ide_poll_ready()) {
        ide_report_error("IDENTIFY");
        return -1;
    }

    /* Read 256 words (512 bytes) of IDENTIFY data */
    for (int i = 0; i < 256; i++) {
        buf[i] = inw(ide_data_base + IDE_DATA);
    }

    /* Extract model string (words 27-46, big-endian in spec) */
    char model[41];
    for (int i = 0; i < 20; i++) {
        uint16_t w = buf[27 + i];
        model[i * 2]     = (w >> 8) & 0xFF;
        model[i * 2 + 1] = w & 0xFF;
    }
    model[40] = '\0';

    /* Trim trailing spaces */
    int len = 40;
    while (len > 0 && model[len - 1] == ' ') len--;
    model[len] = '\0';

    printk("[ide] Drive %d: %s\n", drive, model);
    return 0;
}

/* ==========================================================================
 * READ SECTORS (LBA28)
 * ======================================================================= */
int ide_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint16_t *buf) {
    if (!buf || count == 0)
        return -1;

    if (lba > 0x0FFFFFFF) {
        printk("[ide] LBA 0x%x exceeds LBA28 limit\n", lba);
        return -1;
    }

    if (ide_select_drive(drive) != 0) {
        printk("[ide] Drive %d not present\n", drive);
        return -1;
    }

    uint8_t lba_low  = lba & 0xFF;
    uint8_t lba_mid  = (lba >> 8) & 0xFF;
    uint8_t lba_high = (lba >> 16) & 0xFF;
    uint8_t lba_top  = (lba >> 24) & 0x0F;

    outb(ide_data_base + IDE_DRIVE_SELECT, 0xE0 | (drive << 4) | lba_top);
    ide_delay_400ns();

    outb(ide_data_base + IDE_SECTOR_COUNT, count);
    outb(ide_data_base + IDE_LBA_LOW, lba_low);
    outb(ide_data_base + IDE_LBA_MID, lba_mid);
    outb(ide_data_base + IDE_LBA_HIGH, lba_high);
    outb(ide_data_base + IDE_COMMAND, IDE_CMD_READ_SECTORS);

    for (uint8_t s = 0; s < count; s++) {
        if (!ide_poll_ready()) {
            ide_report_error("READ");
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(ide_data_base + IDE_DATA);
        }
    }

    return 0;
}

/* ==========================================================================
 * WRITE SECTORS (LBA28)
 * ======================================================================= */
int ide_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const uint16_t *buf) {
    if (!buf || count == 0)
        return -1;

    if (lba > 0x0FFFFFFF) {
        printk("[ide] LBA 0x%x exceeds LBA28 limit\n", lba);
        return -1;
    }

    if (ide_select_drive(drive) != 0) {
        printk("[ide] Drive %d not present\n", drive);
        return -1;
    }

    uint8_t lba_low  = lba & 0xFF;
    uint8_t lba_mid  = (lba >> 8) & 0xFF;
    uint8_t lba_high = (lba >> 16) & 0xFF;
    uint8_t lba_top  = (lba >> 24) & 0x0F;

    outb(ide_data_base + IDE_DRIVE_SELECT, 0xE0 | (drive << 4) | lba_top);
    ide_delay_400ns();

    outb(ide_data_base + IDE_SECTOR_COUNT, count);
    outb(ide_data_base + IDE_LBA_LOW, lba_low);
    outb(ide_data_base + IDE_LBA_MID, lba_mid);
    outb(ide_data_base + IDE_LBA_HIGH, lba_high);
    outb(ide_data_base + IDE_COMMAND, IDE_CMD_WRITE_SECTORS);

    for (uint8_t s = 0; s < count; s++) {
        if (!ide_poll_ready()) {
            ide_report_error("WRITE");
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            outw(ide_data_base + IDE_DATA, buf[s * 256 + i]);
        }
    }

    /* Flush cache and verify completion */
    outb(ide_data_base + IDE_COMMAND, IDE_CMD_CACHE_FLUSH);

    if (!ide_wait_bsy()) {
        printk("[ide] Cache flush timeout\n");
        return -1;
    }

    uint8_t status = inb(ide_data_base + IDE_STATUS);
    if (status & (IDE_STATUS_ERR | IDE_STATUS_DF)) {
        ide_report_error("FLUSH");
        return -1;
    }

    return 0;
}