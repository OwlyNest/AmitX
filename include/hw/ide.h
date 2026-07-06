/*
	* hw/ide.h - [Enter description]
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

/* --- Globals ---*/

/* --- Prototypes ---*/
void ide_init(uint16_t data_base, uint16_t ctrl_base);
int ide_identify(uint8_t drive, uint16_t* buf);
int ide_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint16_t* buf);
int ide_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const uint16_t* buf);
#endif