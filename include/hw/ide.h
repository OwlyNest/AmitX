/*
	* hw/ide.h - ATA/ATAPI IDE controller driver interface
	* Author:   amity
	* Date:     Thu Jun 11 15:08:13 2026
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

#ifndef __HW_IDE_H__
#define __HW_IDE_H__
/* --- Includes ---*/
#include <stdint.h>

/* --- Macros ---*/
/* --- Register offsets from data base --- */
#define IDE_DATA             0x00
#define IDE_ERROR            0x01
#define IDE_SECTOR_COUNT     0x02
#define IDE_LBA_LOW          0x03
#define IDE_LBA_MID          0x04
#define IDE_LBA_HIGH         0x05
#define IDE_DRIVE_SELECT     0x06
#define IDE_COMMAND          0x07
#define IDE_STATUS           0x07

/* --- Control register offset from control base --- */
#define IDE_ALT_STATUS       0x00
#define IDE_CONTROL          0x00

/* --- Commands --- */
#define IDE_CMD_READ_SECTORS    0x20
#define IDE_CMD_WRITE_SECTORS   0x30
#define IDE_CMD_IDENTIFY        0xEC
#define IDE_CMD_CACHE_FLUSH     0xE7

/* --- Commands (LBA48) --- */
#define IDE_CMD_READ_SECTORS_EXT   0x24
#define IDE_CMD_WRITE_SECTORS_EXT  0x34
#define IDE_CMD_CACHE_FLUSH_EXT    0xEA

/* --- Status bits --- */
#define IDE_STATUS_ERR       0x01
#define IDE_STATUS_DRQ       0x08
#define IDE_STATUS_SRV       0x10
#define IDE_STATUS_DF        0x20
#define IDE_STATUS_RDY       0x40
#define IDE_STATUS_BSY       0x80

/* --- Default legacy ports --- */
#define IDE_PRIMARY_DATA     0x1F0
#define IDE_PRIMARY_CTRL     0x3F6
#define IDE_SECONDARY_DATA   0x170
#define IDE_SECONDARY_CTRL   0x376

/* --- Typedefs - Structs - Enums ---*/

typedef struct {
    uint16_t general_config;               // Word 0
    uint16_t reserved1[9];                 // Words 1-9
    uint16_t serial_number[10];            // Words 10-19
    uint16_t reserved2[3];                 // Words 20-22
    uint16_t firmware_revision[4];         // Words 23-26
    uint16_t model_number[20];             // Words 27-46
    uint16_t reserved3a[2];                // Words 47-48
    uint16_t capabilities;                 // Word 49
    uint16_t reserved3b[3];                // Words 50-52
    uint16_t field_validity;               // Word 53
    uint16_t reserved3c[6];                // Words 54-59
    uint32_t total_lba28_sectors;          // Words 60-61
    uint16_t reserved4a[18];               // Words 62-79
    uint16_t major_version;                // Word 80
    uint16_t minor_version;                // Word 81
    uint16_t command_set_supported[3];     // Words 82-84
    uint16_t command_set_enabled[3];       // Words 85-87
    uint16_t ultra_dma_modes;              // Word 88
    uint16_t reserved4b[11];               // Words 89-99
    uint64_t total_lba48_sectors;          // Words 100-103
    uint16_t reserved4c[2];                // Words 104-105

    // Word 106: Physical / Logical sector size configuration
    struct {
        uint16_t logical_sectors_per_physical : 4; // Bits 0-3: 2^X multiplier
        uint16_t reserved                     : 8; // Bits 4-11
        uint16_t logical_sector_longer_512    : 1; // Bit 12
        uint16_t physical_sector_longer_logical:1; // Bit 13
        uint16_t must_be_zero                 : 1; // Bit 14
        uint16_t must_be_one                  : 1; // Bit 15
    } sector_size_config;

    uint16_t reserved5[10];                // Words 107-116
    uint32_t logical_sector_size_words;    // Words 117-118
    uint16_t reserved6[137];               // Words 119-255

    // --- Computed / cached fields (NOT part of raw ATA data) ---
    uint32_t logical_sz;
    uint32_t physical_sz;
} __attribute__((__packed__)) ide_identify_t;

/* --- Capability helpers --- */
#define IDE_CAP_LBA          0x0200        // Word 49 bit 9
#define IDE_CAP_DMA          0x0100        // Word 49 bit 8

#define IDE_VALID_CMDSET(w)  (((w) & 0xC000) == 0x4000)
#define IDE_CMDSET_LBA48     0x0400        // Word 83/86/87 bit 10

static inline int ide_supports_lba48(const ide_identify_t *info) {
    return IDE_VALID_CMDSET(info->command_set_supported[1]) && (info->command_set_supported[1] & IDE_CMDSET_LBA48);
}

static inline int ide_lba48_enabled(const ide_identify_t *info) {
    return IDE_VALID_CMDSET(info->command_set_enabled[1]) && (info->command_set_enabled[1] & IDE_CMDSET_LBA48);
}

/* --- Globals ---*/

/* --- Prototypes ---*/
void ide_init(uint16_t data_base, uint16_t ctrl_base);
int ide_identify(uint8_t drive, ide_identify_t *info);
void ide_dump_identify(ide_identify_t *info);
int ide_read_sectors(uint8_t drive, uint32_t lba, uint16_t count, uint16_t *buf);
int ide_write_sectors(uint8_t drive, uint32_t lba, uint16_t count, const uint16_t *buf);
int ide_read_sectors_ext(uint8_t drive, uint64_t lba, uint16_t count, uint16_t *buf);
int ide_write_sectors_ext(uint8_t drive, uint64_t lba, uint16_t count, const uint16_t *buf);
#endif