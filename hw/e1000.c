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
\t* Inline comments:               Column 40, wherever possible, else, whole
multiple of 20
\t* Section headers:               Use 3 '-' characters before and after
\t* Pointer notation:              Next to variable name, not type
\t* Binary operations:             Space around operator
\t* Empty parameter list:          Use (void) instead of ()
\t* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include <arch/x86/idt.h>
#include <arch/x86/interrupts.h>
#include <arch/x86/io.h>
#include <arch/x86/time.h>
#include <hw/e1000.h>
#include <hw/pci.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <internal/phonon_consts.h>
#include <internal/phonon_macros.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <mm/paging.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/
/* --- Globals ---*/
static struct e1000_device e1000_dev;
static int e1000_ready = 0;
static struct rx_packet *rx_queue_head = NULL;
static struct rx_packet *rx_queue_tail = NULL;
/* --- Prototypes ---*/
static uint32_t e1000_read(struct e1000_device *dev, uint32_t reg);
static void e1000_write(struct e1000_device *dev, uint32_t reg, uint32_t val);
static uint16_t e1000_read_eeprom(struct e1000_device *dev, uint8_t addr);
static int e1000_detect_eeprom(struct e1000_device *dev);
static void e1000_read_mac(struct e1000_device *dev);
static void e1000_write_mac(struct e1000_device *dev);
static void e1000_reset(struct e1000_device *dev);
static void e1000_init_rx(struct e1000_device *dev);
static void e1000_init_tx(struct e1000_device *dev);
static int e1000_probe(struct pci_device **out_dev);
/* --- Functions ---*/

static void e1000_rx_enqueue(const uint8_t *data, uint16_t len) {
  struct rx_packet *pkt = (struct rx_packet *)malloc(sizeof(struct rx_packet));
  if (!pkt) {
    e1000_dev.stats.rx_dropped++;
    return;
  }
  pkt->next = NULL;
  pkt->len = len;
  memcpy(pkt->data, data, len);

  if (rx_queue_tail) {
    rx_queue_tail->next = pkt;
  } else {
    rx_queue_head = pkt;
  }
  rx_queue_tail = pkt;
}

struct rx_packet *e1000_rx_dequeue(void) {
  struct rx_packet *pkt = rx_queue_head;
  if (pkt) {
    rx_queue_head = pkt->next;
    if (!rx_queue_head)
      rx_queue_tail = NULL;
  }
  return pkt;
}

static uint32_t e1000_read(struct e1000_device *dev, uint32_t reg) {
  return dev->mmio[reg / 4];
}

static void e1000_write(struct e1000_device *dev, uint32_t reg, uint32_t val) {
  dev->mmio[reg / 4] = val;
}

static uint16_t e1000_read_eeprom(struct e1000_device *dev, uint8_t addr) {
  e1000_write(dev, E1000_REG_EERD, 1 | (addr << 8));
  while (!(e1000_read(dev, E1000_REG_EERD) & (1 << 4)))
    ;
  return (e1000_read(dev, E1000_REG_EERD) >> 16) & 0xFFFF;
}

static int e1000_detect_eeprom(struct e1000_device *dev) {
  uint32_t eecd = e1000_read(dev, E1000_REG_EECD);
  /* BIT 8: EEPresent */
  return (eecd & (1 << 8)) ? 1 : 0;
}

static void e1000_read_mac(struct e1000_device *dev) {
  if (e1000_detect_eeprom(dev)) {
    uint16_t w0 = e1000_read_eeprom(dev, 0);
    uint16_t w1 = e1000_read_eeprom(dev, 1);
    uint16_t w2 = e1000_read_eeprom(dev, 2);

    dev->mac[0] = w0 & 0xFF;
    dev->mac[1] = (w0 >> 8) & 0xFF;
    dev->mac[2] = w1 & 0xFF;
    dev->mac[3] = (w1 >> 8) & 0xFF;
    dev->mac[4] = w2 & 0xFF;
    dev->mac[5] = (w2 >> 8) & 0xFF;
  } else {
    /* Fallback, read from filter regs */
    uint32_t ral = e1000_read(dev, E1000_REG_RAL);
    uint32_t rah = e1000_read(dev, E1000_REG_RAH);

    dev->mac[0] = ral & 0xFF;
    dev->mac[1] = (ral >> 8) & 0xFF;
    dev->mac[2] = (ral >> 16) & 0xFF;
    dev->mac[3] = (ral >> 24) & 0xFF;
    dev->mac[4] = rah & 0xFF;
    dev->mac[5] = (rah >> 8) & 0xFF;
  }
}

static void e1000_write_mac(struct e1000_device *dev) {
  uint32_t ral = dev->mac[0] | (dev->mac[1] << 8) | (dev->mac[2] << 16) |
                 (dev->mac[3] << 24);
  uint32_t rah =
      dev->mac[4] | (dev->mac[5] << 8) | (1 << 31); /* Address Valid bit */

  e1000_write(dev, E1000_REG_RAL, ral);
  e1000_write(dev, E1000_REG_RAH, rah);
}

static void e1000_reset(struct e1000_device *dev) {
  /* Perform device reset */
  e1000_write(dev, E1000_REG_CTRL, E1000_CTRL_RST);
  while (e1000_read(dev, E1000_REG_CTRL) & E1000_CTRL_RST)
    ;

  /* PHY reset can take up to 1ms on real hardware */
  sleep_ms(2); /* 2ms for safety */

  /* mask all interrupts, clear pending */
  e1000_write(dev, E1000_REG_IMC, 0xFFFFFFFF);
  (void)e1000_read(dev, E1000_REG_ICR);
}

static void e1000_init_rx(struct e1000_device *dev) {
  uint32_t ring_pages =
      (sizeof(struct e1000_rx_desc) * E1000_RX_RING_SIZE + FRAME_SIZE - 1) /
      FRAME_SIZE;
  dev->rx_ring =
      (struct e1000_rx_desc *)pmm_alloc_aligned(ring_pages, ring_pages);
  if (!dev->rx_ring) {
    printk("[e1000] Failed to allocate RX ring\n");
    return;
  }
  memset(dev->rx_ring, 0, sizeof(struct e1000_rx_desc) * E1000_RX_RING_SIZE);

  for (int i = 0; i < E1000_RX_RING_SIZE; i++) {
    dev->rx_buffers[i] = (uint8_t *)malloc(2048);
    dev->rx_ring[i].addr = (uint64_t)(uint32_t)dev->rx_buffers[i];
    dev->rx_ring[i].status = 0;
  }

  e1000_write(dev, E1000_REG_RDBAL, (uint32_t)dev->rx_ring);
  e1000_write(dev, E1000_REG_RDBAH, 0);
  e1000_write(dev, E1000_REG_RDLEN,
              sizeof(struct e1000_rx_desc) * E1000_RX_RING_SIZE);
  e1000_write(dev, E1000_REG_RDH, 0);
  e1000_write(dev, E1000_REG_RDT, E1000_RX_RING_SIZE - 1);
  e1000_write(dev, E1000_REG_RCTL,
              E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048 |
                  E1000_RCTL_SECRC);
}

static void e1000_init_tx(struct e1000_device *dev) {
  uint32_t ring_pages =
      (sizeof(struct e1000_tx_desc) * E1000_TX_RING_SIZE + FRAME_SIZE - 1) /
      FRAME_SIZE;
  dev->tx_ring =
      (struct e1000_tx_desc *)pmm_alloc_aligned(ring_pages, ring_pages);
  if (!dev->tx_ring) {
    printk("[e1000] Failed to allocate TX ring\n");
    return;
  }
  memset(dev->tx_ring, 0, sizeof(struct e1000_tx_desc) * E1000_TX_RING_SIZE);

  for (int i = 0; i < E1000_TX_RING_SIZE; i++) {
    dev->tx_buffers[i] = (uint8_t *)malloc(2048);
    dev->tx_ring[i].addr = (uint64_t)(uint32_t)dev->tx_buffers[i];
    dev->tx_ring[i].status = E1000_TXD_STAT_DD;
  }

  e1000_write(dev, E1000_REG_TDBAL, (uint32_t)dev->tx_ring);
  e1000_write(dev, E1000_REG_TDBAH, 0);
  e1000_write(dev, E1000_REG_TDLEN,
              sizeof(struct e1000_tx_desc) * E1000_TX_RING_SIZE);
  e1000_write(dev, E1000_REG_TDH, 0);
  e1000_write(dev, E1000_REG_TDT, 0);
  e1000_write(dev, E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);
  e1000_write(dev, E1000_REG_TIPG, 0x0060200A);
}

static int e1000_probe(struct pci_device **out_dev) {
  static const uint16_t ids[] = {E1000_DEV_82540EM,
                                 E1000_DEV_82545EM_COPPER,
                                 E1000_DEV_82546EB_COPPER,
                                 E1000_DEV_82541EI,
                                 E1000_DEV_82541ER,
                                 E1000_DEV_82541GI,
                                 E1000_DEV_82541PI,
                                 E1000_DEV_82544EI,
                                 E1000_DEV_82544EI_FIBER,
                                 E1000_DEV_82545EM_FIBER,
                                 E1000_DEV_82546EB_FIBER,
                                 E1000_DEV_82546EB_QUAD,
                                 0};

  for (int i = 0; ids[i] != 0; i++) {
    struct pci_device *d = pci_get_device(0x8086, ids[i]);
    if (d) {
      *out_dev = d;
      return 0;
    }
  }
  return -1;
}

static int e1000_irq_handler(interrupt_frame_t *frame) {
  (void)frame;
  uint32_t icr = e1000_read(&e1000_dev, E1000_REG_ICR);

  if (icr == 0) {
    return 0;
  }

  if (icr & (1 << 6)) { /* RXT0 */
    uint8_t buf[2048];
    int n;
    while ((n = e1000_receive(buf, sizeof(buf))) > 0) {
      e1000_rx_enqueue(buf, n);
    }
  }

  if (icr & (1 << 0)) { /* TXDW */
    /* Reclaim descriptors — e1000_send() handles this on next call */
  }

  if (icr & (1 << 2)) { /* LSC */
    e1000_poll_link();
  }
  return 1;
}

static int e1000_init(void) {
  struct pci_device *pci_dev;

  if (e1000_probe(&pci_dev) != 0) {
    printk("[e1000] No compatible device found\n");
    return 1;
  }
  ASSERT(pci_dev);

  e1000_dev.pci = pci_dev;
  printk("[e1000] Found at %02x:%02x.%x, BAR0=0x%x\n", pci_dev->bus,
         pci_dev->device, pci_dev->function, pci_dev->bars[0].base);

  if (pci_dev->bars[0].base & 1) {
    printk("[e1000] Error: BAR0 is I/O, driver needs MMIO\n");
    return 1;
  }

  uintptr_t bar0_phys = pci_dev->bars[0].base & ~0xF;
  size_t mmio_size = 0x20000; /* 128KB covers all e1000 registers */

  /* Map the MMIO region into virtual address space */
  void *mmio_virt =
      vmm_map_physical(bar0_phys, mmio_size, PAGE_WRITABLE | PAGE_NOCACHE);
  if (!mmio_virt) {
    printk("[e1000] Failed to map MMIO region\n");
    return 1;
  }

  e1000_dev.mmio = (volatile uint32_t *)mmio_virt;

  e1000_reset(&e1000_dev);
  e1000_read_mac(&e1000_dev);
  e1000_write_mac(&e1000_dev);

  printk("[e1000] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", e1000_dev.mac[0],
         e1000_dev.mac[1], e1000_dev.mac[2], e1000_dev.mac[3], e1000_dev.mac[4],
         e1000_dev.mac[5]);

  uint32_t ctrl = e1000_read(&e1000_dev, E1000_REG_CTRL);
  e1000_write(&e1000_dev, E1000_REG_CTRL, ctrl | E1000_CTRL_SLU);

  e1000_init_rx(&e1000_dev);
  if (!e1000_dev.rx_ring) {
    printk("[e1000] RX init failed\n");
    return 1;
  }

  e1000_init_tx(&e1000_dev);
  if (!e1000_dev.tx_ring) {
    printk("[e1000] TX init failed\n");
    /* TODO: free RX ring */
    return 1;
  }

  e1000_poll_link();

  e1000_write(&e1000_dev, E1000_REG_IMS,
              (1 << 0)         /* TXDW */
                  | (1 << 6)   /* RXT0 */
                  | (1 << 2)); /* LSC */

  uint8_t irq_line = pci_read_config_byte(pci_dev->bus, pci_dev->device,
                                          pci_dev->function, 0x3C);
  uint8_t irq_pin = pci_read_config_byte(pci_dev->bus, pci_dev->device,
                                         pci_dev->function, 0x3D);

  if (irq_pin == 0) {
    printk("E1000: no INTx pin (maybe MSI/MSI-X only)\n");
    /* fall back to polling or enable MSI later */
  } else {
    printk("E1000: using IRQ%u (pin INT%c)\n", irq_line, 'A' + (irq_pin - 1));

    /* PCI legacy interrupts are level-triggered */
    pic_set_irq_level_triggered(irq_line);

    /* Register on the *shared* vector */
    register_interrupt_handler(32 + irq_line, e1000_irq_handler);

    /* Now it's safe to let the PIC deliver it */
    pic_unmask_irq(irq_line);
  }

  e1000_ready = 1;
  printk("[e1000] Initialization complete\n");
  return 0;
}

kscope_node_t e1000_node = {
    .name = "e1000-nic",
    .id = 0x000E,
    .class = KSCOPE_CLASS_NETWORK,
    .subclass = KSCOPE_SUBCLASS_NETWORK_E1000,
    .requires = (kscope_node_t *[]){&heap_node, &pci_node, &x86_pic_node},
    .require_count = 3,
    .provides = (const char *[]){"net.e1000", "net.eth", "irq.10"},
    .provide_count = 3,
    .init = e1000_init};

int e1000_poll_link(void) {
  if (!e1000_ready) {
    return -1;
  }

  uint32_t status = e1000_read(&e1000_dev, E1000_REG_STATUS);
  int up = (status & E1000_STATUS_LU) ? 1 : 0;

  if (up && !(e1000_dev.flags & E1000_FLAG_LINK_UP)) {
    e1000_dev.flags |= E1000_FLAG_LINK_UP;
    printk("[e1000] Link up\n");
  } else if (!up && (e1000_dev.flags & E1000_FLAG_LINK_UP)) {
    e1000_dev.flags &= ~E1000_FLAG_LINK_UP;
    printk("[e1000] Link down\n");
  }

  return up;
}

const uint8_t *e1000_get_mac(void) {
  return e1000_ready ? e1000_dev.mac : NULL;
}

int e1000_send(const void *data, uint16_t len) {
  if (!e1000_ready) {
    return -1;
  }
  if (!data || len > 2048) {
    return -1;
  }

  struct e1000_tx_desc *desc = &e1000_dev.tx_ring[e1000_dev.tx_tail];

  /* If this descriptor is still owned by hardware, try to reclaim */
  if (!(desc->status & E1000_TXD_STAT_DD)) {
    /* Reclaim completed descriptors from head */
    while (e1000_dev.tx_head != e1000_dev.tx_tail) {
      struct e1000_tx_desc *d = &e1000_dev.tx_ring[e1000_dev.tx_head];
      if (!(d->status & E1000_TXD_STAT_DD)) {
        break;
      }
      e1000_dev.tx_head = (e1000_dev.tx_head + 1) % E1000_TX_RING_SIZE;
    }

    /* After reclaim, is our target descriptor free? */
    if (!(desc->status & E1000_TXD_STAT_DD)) {
      e1000_dev.stats.tx_dropped++;
      return -1; /* Ring full */
    }
  }

  memcpy(e1000_dev.tx_buffers[e1000_dev.tx_tail], data, len);

  desc->length = len;
  desc->cso = 0;
  desc->css = 0;
  desc->special = 0;
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
  desc->status = 0;

  __asm__ volatile("" ::: "memory");

  e1000_dev.tx_tail = (e1000_dev.tx_tail + 1) % E1000_TX_RING_SIZE;
  e1000_write(&e1000_dev, E1000_REG_TDT, e1000_dev.tx_tail);

  e1000_dev.stats.tx_packets++;
  e1000_dev.stats.tx_bytes += len;

  return 0;
}

int e1000_receive(void *buf, uint16_t buf_len) {
  if (!e1000_ready || !buf) {
    return -1;
  }

  if (buf_len == 0) {
    return 0;
  }

  struct e1000_rx_desc *desc = &e1000_dev.rx_ring[e1000_dev.rx_head];

  if (!(desc->status & E1000_RXD_STAT_DD)) {
    return 0; /* No available packet */
  }

  uint16_t len = desc->length;

  if (desc->errors) {
    e1000_dev.stats.rx_errors++;
    /* Consume descriptor to avoid stall */
  }

  if (len > buf_len) {
    len = buf_len;
  }

  memcpy(buf, e1000_dev.rx_buffers[e1000_dev.rx_head], len);

  /* Reset descriptor for hardware reuse */
  desc->status = 0;
  desc->length = 0;
  desc->errors = 0;
  desc->checksum = 0;
  desc->special = 0;

  __asm__ volatile("" ::: "memory");

  uint16_t desc_idx = e1000_dev.rx_head;
  e1000_dev.rx_head = (e1000_dev.rx_head + 1) & (E1000_RX_RING_SIZE - 1);

  e1000_write(&e1000_dev, E1000_REG_RDT, desc_idx);

  e1000_dev.stats.rx_packets++;
  e1000_dev.stats.rx_bytes += len;

  return (int)len;
}

void e1000_shutdown(void) {
  if (!e1000_ready) {
    return;
  }

  e1000_write(&e1000_dev, E1000_REG_IMC, 0xFFFFFFFF);
  e1000_write(&e1000_dev, E1000_REG_RCTL, 0);
  e1000_write(&e1000_dev, E1000_REG_TCTL, 0);

  /* Drain RX queue */
  while (rx_queue_head) {
    struct rx_packet *pkt = rx_queue_head;
    rx_queue_head = pkt->next;
    free(pkt);
  }
  rx_queue_tail = NULL;

  /* Free packet buffers */
  for (int i = 0; i < E1000_TX_RING_SIZE; i++) {
    if (e1000_dev.tx_buffers[i]) {
      free(e1000_dev.tx_buffers[i]);
      e1000_dev.tx_buffers[i] = NULL;
    }
  }
  for (int i = 0; i < E1000_RX_RING_SIZE; i++) {
    if (e1000_dev.rx_buffers[i]) {
      free(e1000_dev.rx_buffers[i]);
      e1000_dev.rx_buffers[i] = NULL;
    }
  }

  uint32_t tx_ring_pages =
      (sizeof(struct e1000_tx_desc) * E1000_TX_RING_SIZE + FRAME_SIZE - 1) /
      FRAME_SIZE;
  uint32_t rx_ring_pages =
      (sizeof(struct e1000_rx_desc) * E1000_RX_RING_SIZE + FRAME_SIZE - 1) /
      FRAME_SIZE;
  pmm_free_frames(e1000_dev.tx_ring, tx_ring_pages);
  pmm_free_frames(e1000_dev.rx_ring, rx_ring_pages);

  e1000_ready = 0;
  e1000_dev.tx_head = 0;
  e1000_dev.tx_tail = 0;
  e1000_dev.rx_head = 0;

  if (e1000_dev.mmio) {
    vmm_unmap_physical((void *)e1000_dev.mmio, 0x20000);
    e1000_dev.mmio = NULL;
  }
}