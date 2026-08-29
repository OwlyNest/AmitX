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
#include <lib/string.h>
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
static void ide_get_sector_geometry(const ide_identify_t *info, uint32_t *out_logical, uint32_t *out_physical);

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

    printk("[ide] Primary channel at I/O 0x%04x, control at 0x%04x\n",
           data_base, ctrl_base);

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
 * Decode logical and physical sector sizes from IDENTIFY data.
 * ======================================================================= */
static void ide_get_sector_geometry(const ide_identify_t *info,
                                    uint32_t *out_logical,
                                    uint32_t *out_physical) {
    uint32_t logical_size = 512;  // Default fallback
    uint32_t physical_size = 512; // Default fallback

    // Validate Word 106 integrity (Bit 14 clear, Bit 15 set)
    if (!info->sector_size_config.must_be_zero &&
        info->sector_size_config.must_be_one) {

        // Check if logical sector size is larger than 512 bytes
        if (info->sector_size_config.logical_sector_longer_512) {
            // Words 117-118 contain size in 16-bit words. Convert to bytes.
            logical_size = info->logical_sector_size_words * 2;
        }

        // Check if physical sector size is larger than logical
        if (info->sector_size_config.physical_sector_longer_logical) {
            uint8_t exponent =
                info->sector_size_config.logical_sectors_per_physical;
            physical_size = logical_size * (1U << exponent);
        } else {
            physical_size = logical_size;
        }
    }

    if (out_logical)  *out_logical  = logical_size;
    if (out_physical) *out_physical = physical_size;
}

/* ==========================================================================
 * IDENTIFY DEVICE
 * ======================================================================= */
int ide_identify(uint8_t drive, ide_identify_t *info) {
    uint16_t buf[256];

    if (!info)
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
        printk("[ide] Drive %d: Unknown signature 0x%02x%02x\n",
               drive, high, mid);
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

    /* ATA strings are big-endian within each word. Swap each word so the
     * bytes appear in the correct order when the uint16_t arrays are
     * interpreted as ASCII strings on little-endian x86. */
    for (int i = 0; i < 10; i++) {
        buf[10 + i] = (buf[10 + i] >> 8) | (buf[10 + i] << 8);
    }
    for (int i = 0; i < 4; i++) {
        buf[23 + i] = (buf[23 + i] >> 8) | (buf[23 + i] << 8);
    }
    for (int i = 0; i < 20; i++) {
        buf[27 + i] = (buf[27 + i] >> 8) | (buf[27 + i] << 8);
    }

    /* IMPORTANT: copy only the raw 512-byte ATA payload. The struct is
     * larger because of the computed tail fields (logical_sz, etc.). */
    memcpy(info, buf, sizeof(buf));

    /* Model string is now normal ASCII in info->model_number */
    char model[41];
    memcpy(model, info->model_number, 40);
    model[40] = '\0';

    /* Trim trailing spaces */
    int len = 40;
    while (len > 0 && model[len - 1] == ' ') len--;
    model[len] = '\0';

    printk("[ide] Drive %d: %s\n", drive, model);

    uint32_t logical_sz = 0;
    uint32_t physical_sz = 0;
    ide_get_sector_geometry(info, &logical_sz, &physical_sz);

    info->logical_sz  = logical_sz;
    info->physical_sz = physical_sz;

    printk("[ide] Sector configurations - Logical: %u bytes, "
           "Physical: %u bytes\n", logical_sz, physical_sz);
    return 0;
}

/* ==========================================================================
 * Copy an ATA string field (already byte-swapped) into a local buffer
 * and trim trailing spaces.
 * ======================================================================= */
static void ide_format_ata_string(char *dst, const void *src, size_t words) {
    size_t maxlen = words * 2;

    memcpy(dst, src, maxlen);

    while (maxlen > 0 && dst[maxlen - 1] == ' ')
        maxlen--;

    dst[maxlen] = '\0';
}

/* ==========================================================================
 * Dump IDENTIFY information
 * ======================================================================= */
void ide_dump_identify(ide_identify_t *info) {
    char str[41];

    printk("\n=== IDE Identity ===\n");

    ide_format_ata_string(str, info->serial_number, 10);
    printk("    Serial Number:       %s\n", str);

    ide_format_ata_string(str, info->firmware_revision, 4);
    printk("    Firmware Revision:   %s\n", str);

    ide_format_ata_string(str, info->model_number, 20);
    printk("    Model Number:        %s\n", str);

    printk("    General Config:      0x%04x\n", info->general_config);
    printk("    Capabilities:        0x%04x\n", info->capabilities);
    printk("    Major Version:       0x%04x\n", info->major_version);
    printk("    Minor Version:       0x%04x\n", info->minor_version);

    printk("    Total LBA28 Sectors: %u\n", info->total_lba28_sectors);

    if (ide_supports_lba48(info)) {
        printk("    LBA48 Supported:     yes\n");
        printk("    Total LBA48 Sectors: %llu\n",
               (unsigned long long)info->total_lba48_sectors);
    } else {
        printk("    LBA48 Supported:     no\n");
    }

    printk("    Logical Sector Size:  %u bytes\n", info->logical_sz);
    printk("    Physical Sector Size: %u bytes\n", info->physical_sz);

    printk("====================\n\n");
}

/* ==========================================================================
 * READ SECTORS (LBA28)
 * ======================================================================= */
int ide_read_sectors(uint8_t drive, uint32_t lba, uint16_t count, uint16_t *buf) {
    if (!buf || count == 0) {
        return -1;
    }

    if (lba > 0x0FFFFFFF || count > 256) {
        return ide_read_sectors_ext(drive, lba, count, buf);
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
int ide_write_sectors(uint8_t drive, uint32_t lba, uint16_t count, const uint16_t *buf) {
    if (!buf || count == 0)
        return -1;

    if (lba > 0x0FFFFFFF || count > 256) {
        ide_write_sectors_ext(drive, lba, count, buf);
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

/* ==========================================================================
 * READ SECTORS (LBA48)
 *
 * LBA48 protocol:
 *   1. Select drive (0x40 | drive)
 *   2. Write sector count HIGH, LBA[31:24], LBA[39:32], LBA[47:40]
 *   3. Write sector count LOW,  LBA[7:0],   LBA[15:8],  LBA[23:16]
 *   4. Issue READ SECTORS EXT (0x24)
 *
 * Sector count is 16-bit; 0 means 65536 sectors.
 * ======================================================================= */
int ide_read_sectors_ext(uint8_t drive, uint64_t lba, uint16_t count, uint16_t *buf) {
    if (!buf || count == 0)
        return -1;

    if (lba > 0x0000FFFFFFFFFFFFULL) {
        printk("[ide] LBA 0x%llx exceeds LBA48 limit\n",
               (unsigned long long)lba);
        return -1;
    }

    if (ide_select_drive(drive) != 0) {
        printk("[ide] Drive %d not present\n", drive);
        return -1;
    }

    uint8_t lba_0_7   = lba & 0xFF;
    uint8_t lba_8_15  = (lba >> 8) & 0xFF;
    uint8_t lba_16_23 = (lba >> 16) & 0xFF;
    uint8_t lba_24_31 = (lba >> 24) & 0xFF;
    uint8_t lba_32_39 = (lba >> 32) & 0xFF;
    uint8_t lba_40_47 = (lba >> 40) & 0xFF;

    uint8_t count_low  = count & 0xFF;
    uint8_t count_high = (count >> 8) & 0xFF;

    outb(ide_data_base + IDE_DRIVE_SELECT, 0x40 | (drive << 4));
    ide_delay_400ns();

    /* High bytes first */
    outb(ide_data_base + IDE_SECTOR_COUNT, count_high);
    outb(ide_data_base + IDE_LBA_LOW,  lba_24_31);
    outb(ide_data_base + IDE_LBA_MID,  lba_32_39);
    outb(ide_data_base + IDE_LBA_HIGH, lba_40_47);

    /* Low bytes second */
    outb(ide_data_base + IDE_SECTOR_COUNT, count_low);
    outb(ide_data_base + IDE_LBA_LOW,  lba_0_7);
    outb(ide_data_base + IDE_LBA_MID,  lba_8_15);
    outb(ide_data_base + IDE_LBA_HIGH, lba_16_23);

    outb(ide_data_base + IDE_COMMAND, IDE_CMD_READ_SECTORS_EXT);

    for (uint16_t s = 0; s < count; s++) {
        if (!ide_poll_ready()) {
            ide_report_error("READ EXT");
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(ide_data_base + IDE_DATA);
        }
    }

    return 0;
}

/* ==========================================================================
 * WRITE SECTORS (LBA48)
 * ======================================================================= */
int ide_write_sectors_ext(uint8_t drive, uint64_t lba, uint16_t count, const uint16_t *buf) {
    if (!buf || count == 0)
        return -1;

    if (lba > 0x0000FFFFFFFFFFFFULL) {
        printk("[ide] LBA 0x%llx exceeds LBA48 limit\n",
               (unsigned long long)lba);
        return -1;
    }

    if (ide_select_drive(drive) != 0) {
        printk("[ide] Drive %d not present\n", drive);
        return -1;
    }

    uint8_t lba_0_7   = lba & 0xFF;
    uint8_t lba_8_15  = (lba >> 8) & 0xFF;
    uint8_t lba_16_23 = (lba >> 16) & 0xFF;
    uint8_t lba_24_31 = (lba >> 24) & 0xFF;
    uint8_t lba_32_39 = (lba >> 32) & 0xFF;
    uint8_t lba_40_47 = (lba >> 40) & 0xFF;

    uint8_t count_low  = count & 0xFF;
    uint8_t count_high = (count >> 8) & 0xFF;

    outb(ide_data_base + IDE_DRIVE_SELECT, 0x40 | (drive << 4));
    ide_delay_400ns();

    /* High bytes first */
    outb(ide_data_base + IDE_SECTOR_COUNT, count_high);
    outb(ide_data_base + IDE_LBA_LOW,  lba_24_31);
    outb(ide_data_base + IDE_LBA_MID,  lba_32_39);
    outb(ide_data_base + IDE_LBA_HIGH, lba_40_47);

    /* Low bytes second */
    outb(ide_data_base + IDE_SECTOR_COUNT, count_low);
    outb(ide_data_base + IDE_LBA_LOW,  lba_0_7);
    outb(ide_data_base + IDE_LBA_MID,  lba_8_15);
    outb(ide_data_base + IDE_LBA_HIGH, lba_16_23);

    outb(ide_data_base + IDE_COMMAND, IDE_CMD_WRITE_SECTORS_EXT);

    for (uint16_t s = 0; s < count; s++) {
        if (!ide_poll_ready()) {
            ide_report_error("WRITE EXT");
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            outw(ide_data_base + IDE_DATA, buf[s * 256 + i]);
        }
    }

    /* Flush cache with LBA48-aware command */
    outb(ide_data_base + IDE_COMMAND, IDE_CMD_CACHE_FLUSH_EXT);

    if (!ide_wait_bsy()) {
        printk("[ide] Cache flush ext timeout\n");
        return -1;
    }

    uint8_t status = inb(ide_data_base + IDE_STATUS);
    if (status & (IDE_STATUS_ERR | IDE_STATUS_DF)) {
        ide_report_error("FLUSH EXT");
        return -1;
    }

    return 0;
}