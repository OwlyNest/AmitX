/*
\t* hw/e1000.c - Intel E1000 NIC Driver
\t* Author:   amity
\t* Date:     Sat Jun 13 01:17:44 2026
\t* Copyright © 2026 OwlyNest
*/

/* --- Styling Instructions ---
\t* Encoding:                      UTF-8, Unix line endings
\t* Text font:                     Monospace
\t* Line width:                    Max 80 characters
\t* Indentation:                   Use 4 spaces
\t* Brace style:                   Same line as control statement
\t* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
\t* Section headers:               Use 3 '-' characters before and after
\t* Pointer notation:              Next to variable name, not type
\t* Binary operations:             Space around operator
\t* Empty parameter list:          Use (void) instead of ()
\t* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <hw/e1000.h>
#include <arch/x86/io.h>
#include <screen/printk.h>
#include <mm/heap.h>
#include <lib/string.h>
#include <hw/pci.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static volatile uint32_t* e1000_mmio;
static struct e1000_tx_desc* tx_ring;
static struct e1000_rx_desc* rx_ring;
static uint8_t* tx_buffers[TX_RING_SIZE];
static uint8_t* rx_buffers[RX_RING_SIZE];
static uint16_t tx_idx = 0;
static uint16_t rx_idx = 0;
/* --- Prototypes ---*/

/* --- Functions ---*/

static uint32_t e1000_read(uint32_t reg) {
	return e1000_mmio[reg / 4];
}

static void e1000_write(uint32_t reg, uint32_t val) {
	e1000_mmio[reg / 4] = val;
}

static uint16_t e1000_read_eeprom(uint8_t addr) {
    e1000_write(E1000_REG_EERD, 1 | (addr << 8));
    while (!(e1000_read(E1000_REG_EERD) & (1 << 4)));
    return (e1000_read(E1000_REG_EERD) >> 16) & 0xFFFF;
}

static int  e1000_init(void) {
    pci_device_t* dev = pci_get_device(0x8086, 0x100e);
    if (!dev) {
        dev = pci_get_device(0x8086, 0x100f);  /* MT */
    }
    if (!dev) {
        dev = pci_get_device(0x8086, 0x1010);  /* T */
    }
    if (!dev) {
        printk("[e1000] No e1000 device found\n");
        return 1;
    }

    printk("[e1000] Found at %02x:%02x.%x, BAR0=0x%x\n",
           dev->bus, dev->device, dev->function, dev->bars[0].base);

    /* Check if BAR0 is I/O or MMIO. Low bit set = I/O space. */
    if (dev->bars[0].base & 1) {
        printk("[e1000] Warning: BAR0 is I/O space, this driver expects MMIO\n");
        /* For I/O space you'd use in/out on (base & ~3).
         * QEMU typically gives MMIO, so we continue with a warning. */
    }

    e1000_mmio = (volatile uint32_t*)(dev->bars[0].base & ~0xF);

    /* Read MAC from EEPROM */
    uint16_t mac_word0 = e1000_read_eeprom(0);
    uint16_t mac_word1 = e1000_read_eeprom(1);
    uint16_t mac_word2 = e1000_read_eeprom(2);

    uint8_t mac[6];
    mac[0] = mac_word0 & 0xFF;
    mac[1] = (mac_word0 >> 8) & 0xFF;
    mac[2] = mac_word1 & 0xFF;
    mac[3] = (mac_word1 >> 8) & 0xFF;
    mac[4] = mac_word2 & 0xFF;
    mac[5] = (mac_word2 >> 8) & 0xFF;

    printk("[e1000] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* Reset */
    uint32_t ctrl = e1000_read(E1000_REG_CTRL);
    e1000_write(E1000_REG_CTRL, ctrl | E1000_CTRL_RST);
    while (e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST);

    /* Disable interrupts */
    e1000_write(E1000_REG_IMC, 0xFFFFFFFF);

    /* Set link up */
    ctrl = e1000_read(E1000_REG_CTRL);
    e1000_write(E1000_REG_CTRL, ctrl | E1000_CTRL_SLU);

    /* Allocate rings (16-byte aligned) */
    tx_ring = (struct e1000_tx_desc*)malloc(
        sizeof(struct e1000_tx_desc) * TX_RING_SIZE + 15);
    tx_ring = (struct e1000_tx_desc*)(
        ((uint32_t)tx_ring + 15) & ~15);
    memset(tx_ring, 0,
           sizeof(struct e1000_tx_desc) * TX_RING_SIZE);

    rx_ring = (struct e1000_rx_desc*)malloc(
        sizeof(struct e1000_rx_desc) * RX_RING_SIZE + 15);
    rx_ring = (struct e1000_rx_desc*)(
        ((uint32_t)rx_ring + 15) & ~15);
    memset(rx_ring, 0,
           sizeof(struct e1000_rx_desc) * RX_RING_SIZE);

    /* Allocate buffers */
    for (int i = 0; i < TX_RING_SIZE; i++) {
        tx_buffers[i] = (uint8_t*)malloc(2048);
        tx_ring[i].addr = (uint64_t)(uint32_t)tx_buffers[i];
        tx_ring[i].status = E1000_TXD_STAT_DD;  /* Done, owned by CPU */
    }

    for (int i = 0; i < RX_RING_SIZE; i++) {
        rx_buffers[i] = (uint8_t*)malloc(2048);
        rx_ring[i].addr = (uint64_t)(uint32_t)rx_buffers[i];
        rx_ring[i].status = 0;  /* Owned by hardware */
    }

    /* Program TX ring */
    e1000_write(E1000_REG_TDBAL, (uint32_t)tx_ring);
    e1000_write(E1000_REG_TDBAH, 0);
    e1000_write(E1000_REG_TDLEN, sizeof(struct e1000_tx_desc) * TX_RING_SIZE);
    e1000_write(E1000_REG_TDH, 0);
    e1000_write(E1000_REG_TDT, 0);

    /* Program RX ring */
    e1000_write(E1000_REG_RDBAL, (uint32_t)rx_ring);
    e1000_write(E1000_REG_RDBAH, 0);
    e1000_write(E1000_REG_RDLEN, sizeof(struct e1000_rx_desc) * RX_RING_SIZE);
    e1000_write(E1000_REG_RDH, 0);
    e1000_write(E1000_REG_RDT, RX_RING_SIZE - 1);

    /* Enable RX: 2048 byte buffers, broadcast, unicast, strip CRC */
    e1000_write(E1000_REG_RCTL,
                  E1000_RCTL_EN
                | E1000_RCTL_BAM
                | E1000_RCTL_BSIZE_2048
                | E1000_RCTL_SECRC);

    /* Enable TX: pad short packets */
    e1000_write(E1000_REG_TCTL,
                  E1000_TCTL_EN
                | E1000_TCTL_PSP);

    printk("[e1000] Initialization complete\n");
    return 0;
}

kscope_node_t e1000_node = {
    .name = "e1000-nic",
    .id = 0x000E,
    .class = KSCOPE_CLASS_NETWORK,
    .subclass = KSCOPE_SUBCLASS_NETWORK_E1000,
    .requires = (kscope_node_t *[]){&heap_node, &pci_node},
    .require_count = 2,
    .provides = (const char *[]){"net.e1000", "net.eth"},
    .provide_count = 2,
    .init = e1000_init
};

int e1000_send(const void* data, uint16_t len) {
    if (len > 2048) {
        return -1;  /* Packet too large for buffer */
    }

    struct e1000_tx_desc* desc = &tx_ring[tx_idx];

    /* Wait for hardware to finish with this descriptor */
    while (!(desc->status & E1000_TXD_STAT_DD)) {
        /* Spin until done. In a real OS you might sleep or
         * check the next descriptor instead of busy-waiting. */
    }

    /* Copy packet data into the pre-allocated buffer */
    memcpy(tx_buffers[tx_idx], data, len);

    /* Fill descriptor */
    desc->length = len;
    desc->cso = 0;
    desc->css = 0;
    desc->special = 0;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    desc->status = 0;  /* Clear DD, hand to hardware */

    /* Advance tail pointer to notify hardware */
    tx_idx = (tx_idx + 1) % TX_RING_SIZE;
    e1000_write(E1000_REG_TDT, tx_idx);

    return 0;
}

int e1000_receive(void* buf, uint16_t buf_len) {
    struct e1000_rx_desc* desc = &rx_ring[rx_idx];

    /* Check if hardware has written a packet here */
    if (!(desc->status & E1000_RXD_STAT_DD)) {
        return 0;  /* No packet available */
    }

    /* Check for errors */
    if (desc->errors) {
        /* Error in this packet — skip it but still advance */
        printk("[e1000] RX error: 0x%02x\n", desc->errors);
    }

    uint16_t len = desc->length;
    if (len > buf_len) {
        len = buf_len;  /* Truncate if user buffer too small */
    }

    /* Copy packet out to caller */
    memcpy(buf, rx_buffers[rx_idx], len);

    /* Reset descriptor for reuse by hardware */
    desc->status = 0;
    desc->length = 0;
    desc->errors = 0;
    desc->checksum = 0;
    desc->special = 0;

    /* Advance our index and update RDT to allow reuse */
    rx_idx = (rx_idx + 1) % RX_RING_SIZE;
    uint16_t rdt = (rx_idx - 1 + RX_RING_SIZE) % RX_RING_SIZE;
    e1000_write(E1000_REG_RDT, rdt);

    return (int)len;
}