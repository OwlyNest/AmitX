/*
	* hw/e1000.h - [Enter description]
	* Author:   amity
	* Date:     Sat Jun 13 01:17:45 2026
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
#ifndef E1000_H
#define E1000_H

#define E1000_REG_CTRL      0x00000
#define E1000_REG_STATUS    0x00008
#define E1000_REG_EECD      0x00010
#define E1000_REG_EERD      0x00014
#define E1000_REG_IMC       0x000D8
#define E1000_REG_RCTL      0x00100
#define E1000_REG_TCTL      0x00400
#define E1000_REG_RDBAL     0x02800
#define E1000_REG_RDBAH     0x02804
#define E1000_REG_RDLEN     0x02808
#define E1000_REG_RDH       0x02810
#define E1000_REG_RDT       0x02818
#define E1000_REG_TDBAL     0x03800
#define E1000_REG_TDBAH     0x03804
#define E1000_REG_TDLEN     0x03808
#define E1000_REG_TDH       0x03810
#define E1000_REG_TDT       0x03818

#define E1000_CTRL_RST      (1 << 26)
#define E1000_CTRL_SLU      (1 << 6)
#define E1000_RCTL_EN       (1 << 1)
#define E1000_RCTL_SBP      (1 << 2)
#define E1000_RCTL_UPE      (1 << 3)
#define E1000_RCTL_MPE      (1 << 4)
#define E1000_RCTL_LPE      (1 << 5)
#define E1000_RCTL_BAM      (1 << 15)
#define E1000_RCTL_BSIZE_2048   (0 << 16)
#define E1000_RCTL_SECRC    (1 << 26)
#define E1000_TCTL_EN       (1 << 1)
#define E1000_TCTL_PSP      (1 << 3)

#define E1000_TXD_CMD_EOP   (1 << 0)
#define E1000_TXD_CMD_IFCS  (1 << 1)
#define E1000_TXD_CMD_RS    (1 << 3)

#define E1000_TXD_STAT_DD   (1 << 0)
#define E1000_RXD_STAT_DD   (1 << 0)

#define RX_RING_SIZE        32
#define TX_RING_SIZE        8
/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));
/* --- Globals ---*/

/* --- Prototypes ---*/
void e1000_init(void);
int e1000_send(const void* data, uint16_t len);
int e1000_receive(void* buf, uint16_t buf_len);
#endif