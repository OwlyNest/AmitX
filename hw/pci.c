/*
 * drivers/pci.c - PCI bus enumeration and device management
 * Author:   amity
 * Date:     Wed Jun 11 12:08:13 2026
 * Copyright © 2026 OwlyNest
 */

#include <hw/pci.h>
#include <arch/x86/io.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <screen/screen.h>
#include <screen/printk.h>
#include <mm/heap.h>
#include <lib/string.h>
#include <stdint.h>

/* ==========================================================================
 * Globals
 * ======================================================================= */
static pci_device_t* pci_device_list = NULL;
static int pci_device_count = 0;
static uint8_t visited_buses[256] = {0};
static uint32_t pci_io_next = 0xC000;     /* start above legacy ports */
static uint32_t pci_mem_next = 0xE0000000; /* start high in 32-bit space */

/* ==========================================================================
 * Low-level config space access
 * ======================================================================= */

static inline void pci_config_addr(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
    uint32_t addr = (1u << 31) | ((uint32_t)bus  << 16) | ((uint32_t)dev  << 11) | ((uint32_t)func <<  8) | (reg & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
}

uint32_t pci_read_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
    pci_config_addr(bus, dev, func, reg);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val) {
    pci_config_addr(bus, dev, func, reg);
    outl(PCI_CONFIG_DATA, val);
}

uint16_t pci_read_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
    uint32_t val = pci_read_config(bus, dev, func, reg & ~3);
    // Shift accurately based on the specific target byte offset inside the 32-bit DWORD
    return (val >> ((reg & 3) * 8)) & 0xFFFF;
}


uint8_t pci_read_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
    uint32_t val = pci_read_config(bus, dev, func, reg & ~3);
    return (val >> ((reg & 3) * 8)) & 0xFF;
}

/* ==========================================================================
 * BAR parsing
 * ======================================================================= */

uint32_t pci_bar_get_size(pci_device_t* dev, uint8_t bar_idx) {
    uint8_t reg = PCI_BAR0 + (bar_idx * 4);
    uint32_t original = pci_read_config(dev->bus, dev->device, dev->function, reg);

    /* Write all 1s to probe size */
    pci_write_config(dev->bus, dev->device, dev->function, reg, 0xFFFFFFFF);
    uint32_t probe = pci_read_config(dev->bus, dev->device, dev->function, reg);

    /* Restore original value */
    pci_write_config(dev->bus, dev->device, dev->function, reg, original);

    if (probe == 0 || probe == 0xFFFFFFFF) return 0;

    uint32_t mask;
    if (probe & PCI_BAR_TYPE_IO) {
        mask = PCI_BAR_IO_ADDR_MASK;
    } else {
        mask = PCI_BAR_MEM_ADDR_MASK;
        /* Check for 64-bit BAR */
        uint8_t mem_type = (probe & PCI_BAR_MEM_TYPE_MASK);
        if (mem_type == PCI_BAR_MEM_TYPE_64 && bar_idx < 5) {
            /* Upper 32 bits */
            uint32_t upper_orig = pci_read_config(dev->bus, dev->device, dev->function, reg + 4);
            pci_write_config(dev->bus, dev->device, dev->function, reg + 4, 0xFFFFFFFF);
            uint32_t upper_probe = pci_read_config(dev->bus, dev->device, dev->function, reg + 4);
            pci_write_config(dev->bus, dev->device, dev->function, reg + 4, upper_orig);

            uint64_t size64 = ((uint64_t)upper_probe << 32) | (probe & mask);
            size64 = ~size64 + 1;
            return (uint32_t)size64;  /* Truncate for 32-bit kernel */
        }
    }

    uint32_t size = probe & mask;
    if (size == 0) return 0;
    size = ~size + 1;
    return size;
}

void pci_parse_bars(pci_device_t* dev) {
    dev->bar_count = 6;
    if (dev->header_type == PCI_HEADER_TYPE_BRIDGE) {
        dev->bar_count = 2;  /* Bridges only have BAR0 and BAR1 */
    }

    for (uint8_t i = 0; i < dev->bar_count; i++) {
        uint8_t reg = PCI_BAR0 + (i * 4);
        uint32_t bar_val = pci_read_config(dev->bus, dev->device, dev->function, reg);

        dev->bars[i].raw = bar_val;
        dev->bars[i].index = i;

        if (bar_val == 0 || bar_val == 0xFFFFFFFF) {
            dev->bars[i].base = 0;
            dev->bars[i].size = 0;
            dev->bars[i].is_io = 0;
            dev->bars[i].is_64 = 0;
            dev->bars[i].is_prefetch = 0;
            continue;
        }

        if (bar_val & PCI_BAR_TYPE_IO) {
            dev->bars[i].is_io = 1;
            dev->bars[i].base = bar_val & PCI_BAR_IO_ADDR_MASK;
            dev->bars[i].is_prefetch = 0;
            dev->bars[i].size = pci_bar_get_size(dev, i); // Get size immediately
        } else {
            dev->bars[i].is_io = 0;
            dev->bars[i].base = bar_val & PCI_BAR_MEM_ADDR_MASK;
            dev->bars[i].is_prefetch = (bar_val & PCI_BAR_MEM_PREFETCH) ? 1 : 0;

            uint8_t mem_type = bar_val & PCI_BAR_MEM_TYPE_MASK;
            if (mem_type == PCI_BAR_MEM_TYPE_64 && i < 5) {
                dev->bars[i].is_64 = 1;
                dev->bars[i].size = pci_bar_get_size(dev, i); // Get size before incrementing
                
                // Zero out the upper pseudo-BAR bookkeeping slot
                dev->bars[i + 1].base = 0;
                dev->bars[i + 1].size = 0;
                
                i++;  /* Now safely skip the upper half */
            } else {
                dev->bars[i].size = pci_bar_get_size(dev, i); // Standard 32-bit sizing
            }
        }

        dev->bars[i].size = pci_bar_get_size(dev, i);
    }
}

uint32_t pci_bar_get_base(pci_bar_t* bar) {
    return bar->base;
}

void pci_bar_enable(pci_device_t* dev, uint8_t bar_idx) {
    if (bar_idx >= dev->bar_count) return;

    uint16_t cmd = pci_read_config_word(dev->bus, dev->device, dev->function, PCI_COMMAND);
    if (dev->bars[bar_idx].is_io) {
        cmd |= PCI_CMD_IO_SPACE;
    } else {
        cmd |= PCI_CMD_MEM_SPACE;
    }
    
    // Always enable Bus Mastering for network adapters to allow DMA transfers
    cmd |= (1u << 2); 

    pci_write_config(dev->bus, dev->device, dev->function, PCI_COMMAND, (pci_read_config(dev->bus, dev->device, dev->function, PCI_COMMAND) & 0xFFFF0000) | cmd);
}

int pci_assign_bar(pci_device_t *dev, uint8_t bar_index) {
    if (bar_index >= dev->bar_count) return -1;

    pci_bar_t *bar = &dev->bars[bar_index];
    if (bar->size == 0) return 0; /* Nothing to assign */
    if (bar->base != 0) return 0; /* BIOS already assigned */

    uint32_t align = bar->size;

    if (bar->is_io) {
        /* I/O space: Round up to size alignment */
        pci_io_next = (pci_io_next + align - 1) & ~(align - 1);
        if (pci_io_next + bar->size > 0x10000) {
            printk("[pci] Out of I/O space for BAR%d on %02x:%02x.%x\n", bar_index, dev->bus, dev->device, dev->function);
         return -1;
        }

        bar->base = pci_io_next;
        pci_io_next += bar->size;
    } else {
        /* Memory space: round up to size alignment */
        pci_mem_next = (pci_mem_next + align - 1) & ~(align - 1);
        if (pci_mem_next + bar->size < pci_mem_next) {  /* overflow */
            printk("[pci] Out of memory space for BAR%d on %02x:%02x.%x\n", bar_index, dev->bus, dev->device, dev->function);
            return -1;
        }
        bar->base = pci_mem_next;
        pci_mem_next += bar->size;
    }

    bar->assigned = 1;

    /* Write assigned address back to BAR */
    uint8_t reg = PCI_BAR0 + (bar_index * 4);
    pci_write_config(dev->bus, dev->device, dev->function, reg, bar->base);
    
    /* For 64-bit BARs, write upper 32 bits (0 for 32-bit kernel) */
    if (bar->is_64 && bar_index < 5) {
        pci_write_config(dev->bus, dev->device, dev->function, reg + 4, 0);
    }
    
    printk("[pci] Assigned BAR%d on %02x:%02x.%x: %s 0x%08x size 0x%x\n", bar_index, dev->bus, dev->device, dev->function, bar->is_io ? "I/O" : "MEM", bar->base, bar->size);
    
    return 0;
}


/* ==========================================================================
 * Capability parsing
 * ======================================================================= */

 void pci_parse_capabilities(pci_device_t* dev) {
    uint16_t status = pci_read_config_word(dev->bus, dev->device, dev->function, PCI_STATUS);
    if (!(status & PCI_STATUS_CAP_LIST)) return;

    // Do NOT mask 0xFC here. Read the raw offset pointer.
    uint8_t cap_ptr = pci_read_config_byte(dev->bus, dev->device, dev->function, PCI_CAP_PTR);
    pci_capability_t* last = NULL;

    // A capability pointer must be 4-byte aligned. Valid pointers are >= 0x40.
    while (cap_ptr >= 0x40 && cap_ptr < 0xFF) {
        uint8_t cap_id = pci_read_config_byte(dev->bus, dev->device, dev->function, cap_ptr);
        uint8_t next_ptr = pci_read_config_byte(dev->bus, dev->device, dev->function, cap_ptr + 1);

        pci_capability_t* cap = (pci_capability_t*)malloc(sizeof(pci_capability_t));
        if (!cap) break; // Use break instead of continue to prevent infinite loops on failure

        cap->id = cap_id;
        cap->next = next_ptr;
        cap->offset = cap_ptr;
        cap->next_cap = NULL;

        if (last) {
            last->next_cap = cap;
        } else {
            dev->capabilities = cap;
        }
        last = cap;

        // Advance directly to the next pointer raw value.
        cap_ptr = next_ptr;
    }
}

int pci_has_capability(pci_device_t* dev, uint8_t cap_id) {
    pci_capability_t* cap = dev->capabilities;
    while (cap) {
        if (cap->id == cap_id) return 1;
        cap = cap->next_cap;
    }
    return 0;
}

/* ==========================================================================
 * Device scanning
 * ======================================================================= */

static pci_device_t* pci_alloc_device(void) {
    pci_device_t* dev = (pci_device_t*)malloc(sizeof(pci_device_t));
    if (!dev) return NULL;

    memset(dev, 0, sizeof(pci_device_t));
    dev->capabilities = NULL;
    return dev;
}

static void pci_read_device_info(pci_device_t* dev, uint8_t bus, uint8_t device, uint8_t function) {
    dev->bus = bus;
    dev->device = device;
    dev->function = function;

    uint32_t reg0 = pci_read_config(bus, device, function, PCI_VENDOR_ID);
    dev->vendor_id = reg0 & 0xFFFF;
    dev->device_id = (reg0 >> 16) & 0xFFFF;

    uint32_t reg2 = pci_read_config(bus, device, function, PCI_COMMAND);
    dev->command = reg2 & 0xFFFF;
    dev->status = (reg2 >> 16) & 0xFFFF;

    uint32_t reg4 = pci_read_config(bus, device, function, PCI_REVISION_ID);
    dev->revision = reg4 & 0xFF;
    dev->prog_if = (reg4 >> 8) & 0xFF;
    dev->subclass = (reg4 >> 16) & 0xFF;
    dev->class_code = (reg4 >> 24) & 0xFF;

    uint32_t reg6 = pci_read_config(bus, device, function, PCI_HEADER_TYPE);
    dev->header_type = (reg6 >> 16) & 0xFF;

    uint32_t reg9 = pci_read_config(bus, device, function, PCI_SUBSYS_VENDOR);
    dev->subsystem_vendor = reg9 & 0xFFFF;
    dev->subsystem_id = (reg9 >> 16) & 0xFFFF;

    uint32_t reg12 = pci_read_config(bus, device, function, PCI_INTERRUPT_LINE);
    dev->interrupt_line = reg12 & 0xFF;
    dev->interrupt_pin = (reg12 >> 8) & 0xFF;

    if (dev->header_type == PCI_HEADER_TYPE_BRIDGE) {
        uint32_t reg_bridge = pci_read_config(bus, device, function, PCI_BRIDGE_BUS_PRIMARY);
        dev->primary_bus = reg_bridge & 0xFF;
        dev->secondary_bus = (reg_bridge >> 8) & 0xFF;
        dev->subordinate_bus = (reg_bridge >> 16) & 0xFF;
    }

    pci_parse_bars(dev);
    pci_parse_capabilities(dev);
}

static void pci_add_device(pci_device_t* dev) {
    dev->next = pci_device_list;
    pci_device_list = dev;
    pci_device_count++;
}

void pci_scan_bus(uint8_t bus) {
    if (visited_buses[bus]) return;
    visited_buses[bus] = 1;
    for (uint8_t dev = 0; dev < 32; dev++) {
        uint32_t id = pci_read_config(bus, dev, 0, PCI_VENDOR_ID);
        uint16_t vendor = id & 0xFFFF;
        if (vendor == 0xFFFF) continue;

        pci_device_t* device = pci_alloc_device();
        if (!device) {
            printk("[pci] Failed to allocate device struct for %02x:%02x.0\n", bus, dev);
            continue;
        }

        pci_read_device_info(device, bus, dev, 0);
        pci_add_device(device);

        /* Multi-function device? */
        if (device->header_type & PCI_HEADER_MULTI_FUNC) {
            for (uint8_t func = 1; func < 8; func++) {
                id = pci_read_config(bus, dev, func, PCI_VENDOR_ID);
                vendor = id & 0xFFFF;
                if (vendor == 0xFFFF) continue;

                pci_device_t* func_dev = pci_alloc_device();
                if (!func_dev) {
                    printk("[pci] Failed to allocate device struct for %02x:%02x.%x\n", bus, dev, func);
                    continue;
                }

                pci_read_device_info(func_dev, bus, dev, func);
                pci_add_device(func_dev);
            }
        }

        /* PCI-to-PCI bridge? Scan behind it */
        if (device->header_type == PCI_HEADER_TYPE_BRIDGE && device->secondary_bus != 0) {
            pci_scan_bus(device->secondary_bus);
        }
    }
}

static int pci_init(void) {
    printk("[pci] Initializing PCI subsystem...\n");
    pci_device_list = NULL;
    pci_device_count = 0;

    pci_scan_bus(0);
    /* Assign unassigned BARs */
    pci_device_t *dev = pci_device_list;
    while (dev) {
        for (uint8_t i = 0; i < dev->bar_count; i++) {
            if (dev->bars[i].size > 0 && dev->bars[i].base == 0) {
                pci_assign_bar(dev, i);
            }
        }
        /* Enable I/O and Memory space */
        pci_set_command(dev, PCI_CMD_IO_SPACE | PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER);
        dev = dev->next;
    }

    printk("[pci] Found %d device(s)\n", pci_device_count);
    return 0;
}

kscope_node_t pci_node = {
    .name = "pci-bus",
    .id = 0x0009,
    .class = KSCOPE_CLASS_CORE,
    .subclass = KSCOPE_SUBCLASS_CORE_PCI,
    .requires = (kscope_node_t *[]){&heap_node},
    .require_count = 1,
    .provides = (const char *[]){"bus.pci", "hw.enumerate"},
    .provide_count = 2,
    .init = pci_init
};

/* ==========================================================================
 * Device lookup
 * ======================================================================= */

pci_device_t* pci_get_device(uint16_t vendor, uint16_t device) {
    pci_device_t* dev = pci_device_list;
    while (dev) {
        if (dev->vendor_id == vendor && dev->device_id == device) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

pci_device_t* pci_get_device_by_class(uint8_t class_code, uint8_t subclass) {
    pci_device_t* dev = pci_device_list;
    while (dev) {
        if (dev->class_code == class_code && dev->subclass == subclass) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

pci_device_t* pci_get_first_device(void) {
    return pci_device_list;
}

int pci_count_devices(void) {
    return pci_device_count;
}

/* ==========================================================================
 * Command / status helpers
 * ======================================================================= */

void pci_set_command(pci_device_t* dev, uint16_t flags) {
    uint32_t cmd_status = pci_read_config(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd_status = (cmd_status & 0xFFFF0000) | (cmd_status & 0xFFFF) | flags;
    pci_write_config(dev->bus, dev->device, dev->function, PCI_COMMAND, cmd_status);
}

void pci_clear_command(pci_device_t* dev, uint16_t flags) {
    uint32_t cmd_status = pci_read_config(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd_status = (cmd_status & 0xFFFF0000) | ((cmd_status & 0xFFFF) & ~flags);
    pci_write_config(dev->bus, dev->device, dev->function, PCI_COMMAND, cmd_status);
}

/* ==========================================================================
 * Human-readable names
 * ======================================================================= */

const char* pci_class_name(uint8_t class_code) {
    switch (class_code) {
        case PCI_CLASS_UNCLASSIFIED:    return "Unclassified";
        case PCI_CLASS_MASS_STORAGE:    return "Mass Storage";
        case PCI_CLASS_NETWORK:         return "Network";
        case PCI_CLASS_DISPLAY:         return "Display";
        case PCI_CLASS_MULTIMEDIA:      return "Multimedia";
        case PCI_CLASS_MEMORY:          return "Memory";
        case PCI_CLASS_BRIDGE:          return "Bridge";
        case PCI_CLASS_COMM:            return "Communication";
        case PCI_CLASS_SYSTEM:          return "System";
        case PCI_CLASS_INPUT:           return "Input";
        case PCI_CLASS_DOCKING:         return "Docking";
        case PCI_CLASS_PROCESSOR:       return "Processor";
        case PCI_CLASS_SERIAL:          return "Serial";
        case PCI_CLASS_WIRELESS:        return "Wireless";
        case PCI_CLASS_INTELLIGENT:     return "Intelligent";
        case PCI_CLASS_SATELLITE:       return "Satellite";
        case PCI_CLASS_ENCRYPTION:      return "Encryption";
        case PCI_CLASS_SIGNAL_PROC:     return "Signal Processing";
        case PCI_CLASS_ACCEL:           return "Accelerator";
        case PCI_CLASS_NON_ESSENTIAL:   return "Non-essential";
        case PCI_CLASS_COPROCESSOR:     return "Coprocessor";
        default:                        return "Unknown";
    }
}

const char* pci_subclass_name(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
        case PCI_CLASS_MASS_STORAGE:
            switch (subclass) {
                case 0x00: return "SCSI";
                case 0x01: return "IDE";
                case 0x02: return "Floppy";
                case 0x03: return "IPI";
                case 0x04: return "RAID";
                case 0x05: return "ATA";
                case 0x06: return "SATA";
                case 0x07: return "SAS";
                case 0x08: return "NVMe";
                default:   return "Other Mass Storage";
            }
        case PCI_CLASS_NETWORK:
            switch (subclass) {
                case 0x00: return "Ethernet";
                case 0x01: return "Token Ring";
                case 0x02: return "FDDI";
                case 0x03: return "ATM";
                case 0x04: return "ISDN";
                case 0x05: return "WorldFip";
                case 0x06: return "PICMG";
                default:   return "Other Network";
            }
        case PCI_CLASS_DISPLAY:
            switch (subclass) {
                case 0x00: return "VGA";
                case 0x01: return "XGA";
                case 0x02: return "3D";
                default:   return "Other Display";
            }
        case PCI_CLASS_BRIDGE:
            switch (subclass) {
                case 0x00: return "Host";
                case 0x01: return "ISA";
                case 0x02: return "EISA";
                case 0x03: return "MCA";
                case 0x04: return "PCI-PCI";
                case 0x05: return "PCMCIA";
                case 0x06: return "NuBus";
                case 0x07: return "CardBus";
                case 0x08: return "RACEway";
                case 0x09: return "Semi-transparent PCI-PCI";
                case 0x0A: return "InfiniBand";
                default:   return "Other Bridge";
            }
        case PCI_CLASS_SERIAL:
            switch (subclass) {
                case 0x00: return "FireWire";
                case 0x01: return "ACCESS";
                case 0x02: return "SSA";
                case 0x03: return "USB";
                case 0x04: return "Fibre Channel";
                case 0x05: return "SMBus";
                case 0x06: return "InfiniBand";
                case 0x07: return "IPMI";
                case 0x08: return "SERCOS";
                case 0x09: return "CANbus";
                default:   return "Other Serial";
            }
        default:
            return pci_class_name(class_code);
    }
}

/* ==========================================================================
 * Debug output
 * ======================================================================= */
void pci_print_pcie_info(pci_device_t *dev) {
    if (!pci_has_capability(dev, PCI_CAP_ID_EXP)) {
        printk("  PCIe: N/A (legacy PCI)\n");
        return;
    }

    pci_capability_t *cap = dev->capabilities;
    while (cap && cap->id != PCI_CAP_ID_EXP)
        cap = cap->next_cap;
    if (!cap)
        return;

    uint8_t off = cap->offset;

    uint16_t pcie_cap = pci_read_config_word(dev->bus, dev->device, dev->function, off + PCI_PCIE_CAP);
    uint8_t dev_type = (pcie_cap >> 4) & 0x0F;

    const char *types[] = {
        "Endpoint", "Legacy Endpoint", "Reserved", "Reserved",
        "Root Port", "Upstream Switch", "Downstream Switch",
        "PCIe-PCI Bridge", "PCI-PCIe Bridge", "Root Complex Integrated",
        "Root Complex Event Collector"
    };

    printk("  PCIe: %s\n", dev_type < 11 ? types[dev_type] : "Unknown");

    uint32_t link_cap = pci_read_config(dev->bus, dev->device, dev->function, off + PCI_PCIE_LINK_CAP);
    uint8_t max_speed = link_cap & 0x0F;
    uint8_t max_width = (link_cap >> 4) & 0x3F;

    uint16_t link_status = pci_read_config_word(dev->bus, dev->device, dev->function, off + PCI_PCIE_LINK_STATUS);
    uint8_t cur_speed = link_status & 0x0F;
    uint8_t cur_width = (link_status >> 4) & 0x3F;

    // Prepend an entry at index 0 to align arrays safely with the spec values
    const char *speeds[] = {"Invalid", "2.5 GT/s", "5.0 GT/s", "8.0 GT/s", "16.0 GT/s", "32.0 GT/s", "64.0 GT/s"};

    // Check bounds cleanly against the maximum index (7 elements total, valid up to index 6)
    const char *max_speed_str = (max_speed > 0 && max_speed < 7) ? speeds[max_speed] : "Unknown";
    const char *cur_speed_str = (cur_speed > 0 && cur_speed < 7) ? speeds[cur_speed] : "Unknown";

    printk("  Link: max %s x%u, current %s x%u\n", max_speed_str, max_width, cur_speed_str, cur_width);

}


void pci_print_device(pci_device_t* dev) {
    printk("PCI %02x:%02x.%x  %04x:%04x  %s (%s)  Rev %02x",
           dev->bus, dev->device, dev->function,
           dev->vendor_id, dev->device_id,
           pci_class_name(dev->class_code),
           pci_subclass_name(dev->class_code, dev->subclass),
           dev->revision,
           dev->interrupt_line);

        if (dev->interrupt_line == 0 && dev->class_code == PCI_CLASS_MASS_STORAGE) {
            printk("  IRQ: ISA (legacy)\n");
        } else {
            printk("  IRQ: %u\n", dev->interrupt_line);
        }

    for (uint8_t i = 0; i < dev->bar_count; i++) {
        if (dev->bars[i].base == 0 && dev->bars[i].size == 0) continue;

        if (dev->bars[i].is_io) {
            printk("  BAR%d: I/O  0x%08x  size=0x%x\n",
                   i, dev->bars[i].base, dev->bars[i].size);
        } else {
            printk("  BAR%d: MEM  0x%08x  size=0x%x  %s\n",
                   i, dev->bars[i].base, dev->bars[i].size,
                   dev->bars[i].is_prefetch ? "prefetchable" : "");
        }
    }

    if (dev->capabilities) {
        printk("  Capabilities:");
        pci_capability_t* cap = dev->capabilities;
        while (cap) {
            printk(" %02x", cap->id);
            cap = cap->next_cap;
        }
        printk("\n");
    }
    pci_print_pcie_info(dev);
}

void pci_print_all_devices(void) {
    printk("\n=== PCI Device List ===\n");
    pci_device_t* dev = pci_device_list;
    while (dev) {
        pci_print_device(dev);
        dev = dev->next;
    }
    printk("=======================\n\n");
}