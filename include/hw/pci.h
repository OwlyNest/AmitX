/*
 * drivers/pci.h - PCI bus enumeration and device management
 * Author:   amity
 * Date:     Wed Jun 11 12:08:00 2026
 * Copyright © 2026 OwlyNest
 */

#ifndef __HW_PCI_H__
#define __HW_PCI_H__

#include <stdint.h>
#include <stddef.h>

/* ==========================================================================
 * PCI Configuration Space registers (offsets in dwords)
 * ======================================================================= */
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION_ID     0x08
#define PCI_PROG_IF         0x09
#define PCI_SUBCLASS        0x0A
#define PCI_CLASS_CODE      0x0B
#define PCI_CACHE_LINE_SIZE 0x0C
#define PCI_LATENCY_TIMER   0x0D
#define PCI_HEADER_TYPE     0x0E
#define PCI_BIST            0x0F
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_CARDBUS_CIS     0x28
#define PCI_SUBSYS_VENDOR   0x2C
#define PCI_SUBSYS_ID       0x2E
#define PCI_ROM_BASE        0x30
#define PCI_CAP_PTR         0x34
#define PCI_INTERRUPT_LINE  0x3C
#define PCI_INTERRUPT_PIN   0x3D
#define PCI_MIN_GRANT       0x3E
#define PCI_MAX_LATENCY     0x3F

/* Type 1 (PCI-to-PCI bridge) specific */
#define PCI_BRIDGE_BUS_PRIMARY   0x18
#define PCI_BRIDGE_BUS_SECONDARY 0x19
#define PCI_BRIDGE_BUS_SUBORDINATE 0x1A
#define PCI_BRIDGE_IO_BASE       0x1C
#define PCI_BRIDGE_IO_LIMIT      0x1D
#define PCI_BRIDGE_MEM_BASE      0x20
#define PCI_BRIDGE_MEM_LIMIT     0x22
#define PCI_BRIDGE_PREF_BASE     0x24
#define PCI_BRIDGE_PREF_LIMIT    0x26

/* ==========================================================================
 * PCI Command register bits
 * ======================================================================= */
#define PCI_CMD_IO_SPACE        0x0001
#define PCI_CMD_MEM_SPACE       0x0002
#define PCI_CMD_BUS_MASTER      0x0004
#define PCI_CMD_SPECIAL_CYCLES  0x0008
#define PCI_CMD_MEM_WR_INV      0x0010
#define PCI_CMD_VGA_PAL_SNOOP   0x0020
#define PCI_CMD_PARITY_ERR      0x0040
#define PCI_CMD_RESERVED        0x0080
#define PCI_CMD_SERR_ENABLE     0x0100
#define PCI_CMD_FAST_BACK2BACK  0x0200
#define PCI_CMD_INT_DISABLE     0x0400

/* ==========================================================================
 * PCI Status register bits
 * ======================================================================= */
#define PCI_STATUS_CAP_LIST     0x0010
#define PCI_STATUS_66MHZ        0x0020
#define PCI_STATUS_FAST_BACK    0x0080
#define PCI_STATUS_MASTER_PARITY 0x0100
#define PCI_STATUS_DEVSEL_MASK  0x0600
#define PCI_STATUS_SIG_TARGET_ABORT 0x0800
#define PCI_STATUS_REC_TARGET_ABORT 0x1000
#define PCI_STATUS_REC_MASTER_ABORT 0x2000
#define PCI_STATUS_SIG_SYS_ERR  0x4000
#define PCI_STATUS_DETECTED_PARITY 0x8000

/* ==========================================================================
 * Header types
 * ======================================================================= */
#define PCI_HEADER_TYPE_NORMAL   0x00
#define PCI_HEADER_TYPE_BRIDGE   0x01
#define PCI_HEADER_TYPE_CARDBUS  0x02
#define PCI_HEADER_MULTI_FUNC    0x80

/* ==========================================================================
 * BAR types
 * ======================================================================= */
#define PCI_BAR_TYPE_MASK       0x00000001
#define PCI_BAR_TYPE_IO         0x00000001
#define PCI_BAR_TYPE_MEM        0x00000000

/* Memory BAR specific */
#define PCI_BAR_MEM_TYPE_MASK   0x00000006
#define PCI_BAR_MEM_TYPE_32     0x00000000
#define PCI_BAR_MEM_TYPE_1M     0x00000002
#define PCI_BAR_MEM_TYPE_64     0x00000004
#define PCI_BAR_MEM_PREFETCH    0x00000008
#define PCI_BAR_MEM_ADDR_MASK   0xFFFFFFF0

/* I/O BAR specific */
#define PCI_BAR_IO_ADDR_MASK    0xFFFFFFFC

/* ==========================================================================
 * Capability IDs
 * ======================================================================= */
#define PCI_CAP_ID_PM           0x01  /* Power Management */
#define PCI_CAP_ID_AGP          0x02  /* AGP */
#define PCI_CAP_ID_VPD          0x03  /* Vital Product Data */
#define PCI_CAP_ID_SLOTID       0x04  /* Slot Identification */
#define PCI_CAP_ID_MSI          0x05  /* Message Signalled Interrupts */
#define PCI_CAP_ID_CHSWP        0x06  /* CompactPCI HotSwap */
#define PCI_CAP_ID_PCIX         0x07  /* PCI-X */
#define PCI_CAP_ID_HT           0x08  /* HyperTransport */
#define PCI_CAP_ID_VNDR         0x09  /* Vendor Specific */
#define PCI_CAP_ID_DBG          0x0A  /* Debug port */
#define PCI_CAP_ID_CCRC         0x0B  /* CompactPCI Central Resource Control */
#define PCI_CAP_ID_SHPC         0x0C  /* PCI Standard Hot-Plug Controller */
#define PCI_CAP_ID_SSVID        0x0D  /* Bridge subsystem vendor/device ID */
#define PCI_CAP_ID_AGP3         0x0E  /* AGP Target PCI-PCI bridge */
#define PCI_CAP_ID_SECDEV       0x0F  /* Secure Device */
#define PCI_CAP_ID_EXP          0x10  /* PCI Express */
#define PCI_CAP_ID_MSIX         0x11  /* MSI-X */
#define PCI_CAP_ID_SATA         0x12  /* SATA Data/Index Conf. */
#define PCI_CAP_ID_AF           0x13  /* PCI Advanced Features */

/* ==========================================================================
 * Class codes (high-level categories)
 * ======================================================================= */
#define PCI_CLASS_UNCLASSIFIED   0x00
#define PCI_CLASS_MASS_STORAGE   0x01
#define PCI_CLASS_NETWORK        0x02
#define PCI_CLASS_DISPLAY        0x03
#define PCI_CLASS_MULTIMEDIA     0x04
#define PCI_CLASS_MEMORY         0x05
#define PCI_CLASS_BRIDGE         0x06
#define PCI_CLASS_COMM           0x07
#define PCI_CLASS_SYSTEM         0x08
#define PCI_CLASS_INPUT          0x09
#define PCI_CLASS_DOCKING        0x0A
#define PCI_CLASS_PROCESSOR      0x0B
#define PCI_CLASS_SERIAL         0x0C
#define PCI_CLASS_WIRELESS       0x0D
#define PCI_CLASS_INTELLIGENT    0x0E
#define PCI_CLASS_SATELLITE      0x0F
#define PCI_CLASS_ENCRYPTION     0x10
#define PCI_CLASS_SIGNAL_PROC    0x11
#define PCI_CLASS_ACCEL          0x12
#define PCI_CLASS_NON_ESSENTIAL  0x13
#define PCI_CLASS_COPROCESSOR    0x40
#define PCI_CLASS_UNASSIGNED     0xFF

/* ==========================================================================
 * PCI I/O ports
 * ======================================================================= */
#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

/* ==========================================================================
 * PCIe capability register offsets
 * ======================================================================= */
#define PCI_PCIE_CAP         0x02
#define PCI_PCIE_DEV_CAP     0x04
#define PCI_PCIE_DEV_CTRL    0x08
#define PCI_PCIE_LINK_CAP    0x0C
#define PCI_PCIE_LINK_CTRL   0x10
#define PCI_PCIE_LINK_STATUS 0x12
/* --- Typedefs - Structs - Enums ---*/
/* ==========================================================================
 * BAR descriptor
 * ======================================================================= */
typedef struct {
    uint32_t raw;
    uintptr_t base;         /* current or assigned base */
    size_t   size;
    uint8_t  is_io;
    uint8_t  is_64;
    uint8_t  is_prefetch;
    uint8_t  index;
    uint8_t  assigned;      /* NEW: 1 if kernel assigned this address */
} pci_bar_t;

/* ==========================================================================
 * PCI capability entry
 * ======================================================================= */
typedef struct pci_capability {
    uint8_t  id;
    uint8_t  next;
    uint8_t  offset;
    struct pci_capability* next_cap;
} pci_capability_t;

/* ==========================================================================
 * PCI device descriptor
 * ======================================================================= */
typedef struct pci_device {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;

    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;

    uint8_t  revision;
    uint8_t  prog_if;
    uint8_t  subclass;
    uint8_t  class_code;

    uint8_t  header_type;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;

    uint32_t subsystem_vendor;
    uint32_t subsystem_id;

    pci_bar_t bars[6];
    uint8_t   bar_count;

    pci_capability_t* capabilities;

    /* Bridge-specific (if header_type == 1) */
    uint8_t  primary_bus;
    uint8_t  secondary_bus;
    uint8_t  subordinate_bus;

    struct pci_device* next;
} pci_device_t;

/* --- Prototypes ---*/
/* Low-level config space access */
uint32_t pci_read_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
void     pci_write_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val);
uint16_t pci_read_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
uint8_t  pci_read_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
void pci_write_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint16_t val);
/* Device management */
void        pci_scan_bus(uint8_t bus);
pci_device_t* pci_get_device(uint16_t vendor, uint16_t device);
pci_device_t* pci_get_device_by_class(uint8_t class_code, uint8_t subclass);
pci_device_t* pci_get_first_device(void);
int         pci_count_devices(void);

/* BAR handling */
void pci_parse_bars(pci_device_t* dev);
uintptr_t pci_bar_get_base(pci_bar_t* bar);
size_t    pci_bar_get_size(pci_device_t* dev, uint8_t bar_idx);
void pci_bar_enable(pci_device_t* dev, uint8_t bar_idx);

/* Capability handling */
void pci_parse_capabilities(pci_device_t* dev);
int  pci_has_capability(pci_device_t* dev, uint8_t cap_id);

/* Debug / info */
void pci_print_device(pci_device_t* dev);
void pci_print_all_devices(void);
const char* pci_class_name(uint8_t class_code);
const char* pci_subclass_name(uint8_t class_code, uint8_t subclass);

/* Command/status */
void pci_set_command(pci_device_t* dev, uint16_t flags);
void pci_clear_command(pci_device_t* dev, uint16_t flags);

#endif