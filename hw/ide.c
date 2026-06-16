/*
	* hw/ide.c - [Enter description]
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
#include <screen/screen.h>
#include <screen/printk.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static uint16_t ide_data_base = 0x1F0;
static uint16_t ide_ctrl_base = 0x3F6;
/* --- Prototypes ---*/

/* --- Functions ---*/
static int ide_wait_bsy(void) {
    uint32_t timeout = 1000000;
    while ((inb(ide_data_base + IDE_STATUS) & IDE_STATUS_BSY) && timeout--) {
        /* spin */
    }
    return timeout != 0;
}

static int ide_wait_drq(void) {
    uint32_t timeout = 1000000;
    while (timeout--) {
        uint8_t status = inb(ide_data_base + IDE_STATUS);
        if (status & IDE_STATUS_ERR) return 0;
        if (status & IDE_STATUS_DRQ) return 1;
    }
    return 0;
}

void ide_init(uint16_t data_base, uint16_t ctrl_base) {
    ide_data_base = data_base;
    ide_ctrl_base = ctrl_base;
    printk("[ide] Primary channel at I/O 0x%x, control at 0x%x\n",
           data_base, ctrl_base);

    /* Disable interrupts, software reset */
    outb(ide_ctrl_base, 0x04 | 0x02);  /* SRST | nIEN */
    for (volatile int i = 0; i < 100000; i++);
    outb(ide_ctrl_base, 0x02);         /* nIEN only */
    for (volatile int i = 0; i < 100000; i++);

    /* Wait for BSY to clear */
    ide_wait_bsy();
}

int ide_identify(uint8_t drive, uint16_t* buf) {
    /* Select drive, wait for it to settle */
    outb(ide_data_base + IDE_DRIVE_SELECT, 0xA0 | (drive << 4));
    for (volatile int i = 0; i < 1000; i++);

    /* Send IDENTIFY parameters (all zeros) */
    outb(ide_data_base + IDE_SECTOR_COUNT, 0);
    outb(ide_data_base + IDE_LBA_LOW, 0);
    outb(ide_data_base + IDE_LBA_MID, 0);
    outb(ide_data_base + IDE_LBA_HIGH, 0);

    /* Send command */
    outb(ide_data_base + IDE_COMMAND, IDE_CMD_IDENTIFY);

    /* Poll for status — if no drive, status stays 0 */
    uint8_t status = 0;
    for (uint32_t i = 0; i < 100000; i++) {
        status = inb(ide_data_base + IDE_STATUS);
        if (status != 0) break;
    }
    if (status == 0) {
        printk("[ide] No drive %d\n", drive);
        return -1;
    }

    /* Wait for BSY to clear */
    if (!ide_wait_bsy()) {
        printk("[ide] Drive %d: BSY timeout (status=0x%02x)\n", drive, status);
        return -1;
    }

    /* Check for ATAPI/SATA signature */
    uint8_t mid  = inb(ide_data_base + IDE_LBA_MID);
    uint8_t high = inb(ide_data_base + IDE_LBA_HIGH);
    if (mid || high) {
        printk("[ide] Drive %d: ATAPI/SATA sig 0x%02x%02x\n", drive, high, mid);
        return -1;
    }

    /* Check ERR before DRQ */
    status = inb(ide_data_base + IDE_STATUS);
    if (status & IDE_STATUS_ERR) {
        printk("[ide] Drive %d: ERR set after IDENTIFY\n", drive);
        return -1;
    }

    if (!ide_wait_drq()) {
        printk("[ide] Drive %d: DRQ timeout\n", drive);
        return -1;
    }

    /* Read 256 words (512 bytes) of IDENTIFY data */
    for (int i = 0; i < 256; i++) {
        buf[i] = inw(ide_data_base + IDE_DATA);
    }

    /* Extract and byte-swap model string (words 27-46, big-endian) */
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

int ide_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint16_t* buf) {
    outb(ide_data_base + IDE_DRIVE_SELECT,
         0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    outb(ide_data_base + IDE_SECTOR_COUNT, count);
    outb(ide_data_base + IDE_LBA_LOW, lba & 0xFF);
    outb(ide_data_base + IDE_LBA_MID, (lba >> 8) & 0xFF);
    outb(ide_data_base + IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ide_data_base + IDE_COMMAND, IDE_CMD_READ_SECTORS);

    for (uint8_t s = 0; s < count; s++) {
        if (!ide_wait_bsy()) {
            printk("[ide] Read BSY timeout on sector %d\n", s);
            return -1;
        }
        if (!ide_wait_drq()) {
            printk("[ide] Read DRQ timeout on sector %d\n", s);
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(ide_data_base + IDE_DATA);
        }
    }

    return 0;
}

int ide_write_sectors(uint8_t drive, uint32_t lba, uint8_t count,
                      const uint16_t* buf) {
    outb(ide_data_base + IDE_DRIVE_SELECT,
         0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    outb(ide_data_base + IDE_SECTOR_COUNT, count);
    outb(ide_data_base + IDE_LBA_LOW, lba & 0xFF);
    outb(ide_data_base + IDE_LBA_MID, (lba >> 8) & 0xFF);
    outb(ide_data_base + IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ide_data_base + IDE_COMMAND, IDE_CMD_WRITE_SECTORS);

    for (uint8_t s = 0; s < count; s++) {
        if (!ide_wait_bsy()) {
            printk("[ide] Write BSY timeout on sector %d\n", s);
            return -1;
        }
        if (!ide_wait_drq()) {
            printk("[ide] Write DRQ timeout on sector %d\n", s);
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            outw(ide_data_base + IDE_DATA, buf[s * 256 + i]);
        }
    }

    /* Flush cache */
    outb(ide_data_base + IDE_COMMAND, IDE_CMD_CACHE_FLUSH);
    ide_wait_bsy();

    return 0;
}