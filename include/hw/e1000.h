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
#define E1000_REG_ICR       0x000C0
#define E1000_REG_IMS       0x000D0
#define E1000_REG_IMC       0x000D8
#define E1000_REG_RCTL      0x00100
#define E1000_REG_TCTL      0x00400
#define E1000_REG_TIPG      0x00410
#define E1000_REG_RDBAL     0x02800
#define E1000_REG_RDBAH     0x02804
#define E1000_REG_RDLEN     0x02808
#define E1000_REG_RDH       0x02810
#define E1000_REG_RDT       0x02818
#define E1000_REG_RDTR		0x02820
#define E1000_REG_TDBAL     0x03800
#define E1000_REG_TDBAH     0x03804
#define E1000_REG_TDLEN     0x03808
#define E1000_REG_TDH       0x03810
#define E1000_REG_TDT       0x03818
#define E1000_REG_RAL 		0x05400
#define E1000_REG_RAH 		0x05404
#define E1000_REG_MTA 		0x05200

#define E1000_CTRL_RST      (1 << 26)
#define E1000_CTRL_SLU      (1 << 6)
#define E1000_CTRL_FD       (1 << 0)
#define E1000_CTRL_ASDE     (1 << 5)
#define E1000_CTRL_LRST     (1 << 3)

#define E1000_STATUS_LU     (1 << 1)
#define E1000_STATUS_FD     (1 << 0)
#define E1000_STATUS_SPEED  0xC0

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
#define E1000_RDX_STAT_EOP  (1 << 1)

#define E1000_RX_RING_SIZE        32
#define E1000_TX_RING_SIZE        8

#define E1000_DEV_82540EM        0x100E
#define E1000_DEV_82545EM_COPPER 0x100F
#define E1000_DEV_82546EB_COPPER 0x1010
#define E1000_DEV_82541EI 		 0x1017
#define E1000_DEV_82541ER 		 0x1078
#define E1000_DEV_82541GI        0x1076
#define E1000_DEV_82541PI 		 0x1079
#define E1000_DEV_82544EI        0x1008
#define E1000_DEV_82544EI_FIBER  0x1009
#define E1000_DEV_82545EM_FIBER  0x100B
#define E1000_DEV_82546EB_FIBER  0x1011
#define E1000_DEV_82546EB_QUAD   0x1019

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

struct e1000_stats {
	uint64_t tx_packets;
	uint64_t tx_bytes;
	uint64_t tx_dropped;
	uint64_t rx_packets;
	uint64_t rx_bytes;
	uint64_t rx_errors;
	uint64_t rx_dropped;
};

struct e1000_device {
	volatile uint32_t *mmio;
	struct pci_device *pci;
	uint8_t mac[6];

	struct e1000_tx_desc *tx_ring;
	struct e1000_rx_desc *rx_ring;
	uint8_t *tx_buffers[E1000_TX_RING_SIZE];
	uint8_t *rx_buffers[E1000_RX_RING_SIZE];

	uint16_t tx_head; /* Next desc to reclaim */
	uint16_t tx_tail; /* Next desc to use */
	uint16_t rx_head; /* Next desc to process */

	struct e1000_stats stats;
	uint32_t flags;
#define E1000_FLAG_LINK_UP (1 << 0)
};

struct rx_packet {
	struct rx_packet *next;
    uint16_t len;
    uint8_t data[2048];
};

/* --- Globals ---*/

/* --- Prototypes ---*/
struct rx_packet *e1000_rx_dequeue(void);
int e1000_send(const void* data, uint16_t len);
int e1000_receive(void* buf, uint16_t buf_len);
int e1000_poll_link(void);
const uint8_t *e1000_get_mac(void);
void e1000_shutdown(void);
const struct e1000_stats *e1000_get_stats(void);
#endif