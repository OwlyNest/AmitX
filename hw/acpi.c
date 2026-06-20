/*
	* hw/acpi.c - ACPI subsystem
	* Author:   amity
	* Date:     Thu Jun 11 13:12:53 2026
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
#include <stddef.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <hw/acpi.h>
#include <arch/x86/io.h>
#include <screen/screen.h>
#include <mm/heap.h>
#include <screen/printk.h>
#include <internal/amitx_consts.h>
#include <internal/virtmem.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static acpi_state_t acpi_state = { 0 };
static madt_parsed_t madt_parsed = { 0 };

/* --- Prototypes ---*/
static int acpi_checksum_valid(const uint8_t *data, size_t len);
static int acpi_validate_rsdp(acpi_rsdp_t *rsdp);
static int acpi_validate_sdt(acpi_sdt_header_t *header,
                             const char *expected_sig, uint32_t max_len);
static acpi_rsdp_t* acpi_find_rsdp_method1(void);
static acpi_rsdp_t* acpi_find_rsdp_method2(void);
static acpi_rsdp_t* acpi_find_rsdp(void);
static void* acpi_find_table_in_rsdt(acpi_rsdt_t *rsdt,
                                      const char *signature);
static void* acpi_find_table_in_xsdt(acpi_xsdt_t *xsdt,
                                      const char *signature);
static uint16_t acpi_read_pm1a_cnt(void);
static void acpi_write_pm1a_cnt(uint16_t val);
static void acpi_enable(void);

/* --- Functions ---*/

/* ==========================================================================
 * ACPI checksum: sum of all bytes must equal zero
 * ======================================================================= */
static int acpi_checksum_valid(const uint8_t *data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum += data[i];
    return sum == 0;
}

/* ==========================================================================
 * Validate RSDP
 *  - Signature match
 *  - ACPI 1.0 checksum (first 20 bytes)
 *  - If revision >= 2: length sanity + extended checksum
 * ======================================================================= */
static int acpi_validate_rsdp(acpi_rsdp_t *rsdp) {
    if (!rsdp)
        return 0;

    if (memcmp(rsdp->signature, ACPI_RSDP_SIG, ACPI_RSDP_SIG_LEN) != 0)
        return 0;

    if (!acpi_checksum_valid((uint8_t *)rsdp, 20))
        return 0;

    if (rsdp->revision >= 2) {
        if (rsdp->length < 36) {
            printk("[acpi] RSDP rev %u but length %u < 36\n",
                   rsdp->revision, rsdp->length);
            return 0;
        }
        if (!acpi_checksum_valid((uint8_t *)rsdp, rsdp->length))
            return 0;
    }

    return 1;
}

/* ==========================================================================
 * Validate generic SDT header
 *  - Optional signature match
 *  - Minimum length sanity
 *  - Optional maximum length cap (0 = no cap)
 *  - Checksum over entire table
 * ======================================================================= */
static int acpi_validate_sdt(acpi_sdt_header_t *header,
                             const char *expected_sig, uint32_t max_len) {
    if (!header)
        return 0;

    if (expected_sig &&
        memcmp(header->signature, expected_sig, 4) != 0)
        return 0;

    if (header->length < sizeof(acpi_sdt_header_t))
        return 0;

    if (max_len && header->length > max_len)
        return 0;

    if (!acpi_checksum_valid((uint8_t *)header, header->length))
        return 0;

    return 1;
}

/* ==========================================================================
 * RSDP search — Method 1: EBDA (Extended BIOS Data Area)
 * BDA is at physical 0x400; EBDA segment at 0x40E:0x40F.
 * ======================================================================= */
static acpi_rsdp_t* acpi_find_rsdp_method1(void) {
    uint16_t *ebda_seg_ptr = (uint16_t *)auto_virt(0x40E);
    uint16_t ebda_segment = *ebda_seg_ptr;
    uint32_t ebda_addr = (uint32_t)ebda_segment << 4;

    if (ebda_segment == 0 || ebda_addr < 0x80000 ||
        ebda_addr > 0xA0000)
        return NULL;

    for (uint32_t addr = ebda_addr;
         addr < ebda_addr + 1024;
         addr += 16) {
        acpi_rsdp_t *candidate = (acpi_rsdp_t *)auto_virt(addr);
        if (acpi_validate_rsdp(candidate))
            return candidate;
    }
    return NULL;
}

/* ==========================================================================
 * RSDP search — Method 2: BIOS ROM region (0xE0000 - 0xFFFFF)
 * ======================================================================= */
static acpi_rsdp_t* acpi_find_rsdp_method2(void) {
    for (uint32_t addr = 0xE0000;
         addr < 0x100000;
         addr += 16) {
        acpi_rsdp_t *candidate = (acpi_rsdp_t *)auto_virt(addr);
        if (acpi_validate_rsdp(candidate))
            return candidate;
    }
    return NULL;
}

/* ==========================================================================
 * Find RSDP — tries method 1 first, then method 2
 * ======================================================================= */
static acpi_rsdp_t* acpi_find_rsdp(void) {
    acpi_rsdp_t *rsdp = acpi_find_rsdp_method1();
    if (rsdp) {
        printk("[acpi] RSDP found via EBDA at 0x%08x\n",
               (uint32_t)rsdp);
        return rsdp;
    }

    rsdp = acpi_find_rsdp_method2();
    if (rsdp) {
        printk("[acpi] RSDP found via BIOS ROM search at 0x%08x\n",
               (uint32_t)rsdp);
        return rsdp;
    }

    return NULL;
}

/* ==========================================================================
 * Walk RSDT to find a table by signature
 * ======================================================================= */
static void* acpi_find_table_in_rsdt(acpi_rsdt_t *rsdt,
                                      const char *signature) {
    if (!rsdt)
        return NULL;

    if (rsdt->header.length < sizeof(acpi_sdt_header_t))
        return NULL;

    uint32_t num_entries =
        (rsdt->header.length - sizeof(acpi_sdt_header_t)) / 4;

    for (uint32_t i = 0; i < num_entries; i++) {
        acpi_sdt_header_t *header =
            (acpi_sdt_header_t *)auto_virt(rsdt->entries[i]);
        if (acpi_validate_sdt(header, signature, 0x100000))
            return header;
    }
    return NULL;
}

/* ==========================================================================
 * Walk XSDT to find a table by signature
 * On 32-bit kernels, high 32 bits of each entry must be zero.
 * ======================================================================= */
static void* acpi_find_table_in_xsdt(acpi_xsdt_t *xsdt,
                                      const char *signature) {
    if (!xsdt)
        return NULL;

    if (xsdt->header.length < sizeof(acpi_sdt_header_t))
        return NULL;

    uint32_t num_entries =
        (xsdt->header.length - sizeof(acpi_sdt_header_t)) / 8;

    for (uint32_t i = 0; i < num_entries; i++) {
        uint64_t entry_phys = xsdt->entries[i];

        if ((entry_phys >> 32) != 0)
            continue;

        acpi_sdt_header_t *header =
            (acpi_sdt_header_t *)auto_virt((uint32_t)entry_phys);
        if (acpi_validate_sdt(header, signature, 0x100000))
            return header;
    }
    return NULL;
}

/* ==========================================================================
 * Public table finder
 * ======================================================================= */
void* acpi_find_table(const char *signature) {
    if (!acpi_state.rsdp)
        return NULL;

    if (acpi_state.acpi_version >= 2 &&
        acpi_state.rsdp->xsdt_addr != 0) {
        acpi_xsdt_t *xsdt =
            (acpi_xsdt_t *)auto_virt(
                (uint32_t)acpi_state.rsdp->xsdt_addr);
        return acpi_find_table_in_xsdt(xsdt, signature);
    } else {
        acpi_rsdt_t *rsdt =
            (acpi_rsdt_t *)auto_virt(
                (uint32_t)acpi_state.rsdp->rsdt_addr);
        return acpi_find_table_in_rsdt(rsdt, signature);
    }
}

/* ==========================================================================
 * ACPI initialization
 * ======================================================================= */
static int acpi_init(void) {
    printk("[acpi] Initializing ACPI subsystem...\n");

    acpi_state.rsdp = acpi_find_rsdp();
    if (!acpi_state.rsdp) {
        printk("[acpi] ERROR: RSDP not found\n");
        return -1;
    }

    acpi_state.acpi_version =
        (acpi_state.rsdp->revision >= 2) ? 2 : 1;
    printk("[acpi] ACPI version %d.0 detected\n",
           acpi_state.acpi_version);

    acpi_state.fadt =
        (acpi_fadt_t *)acpi_find_table(ACPI_FADT_SIG);
    if (!acpi_state.fadt) {
        printk("[acpi] ERROR: FADT not found\n");
        return -1;
    }
    printk("[acpi] FADT found at 0x%08x\n",
           (uint32_t)acpi_state.fadt);

    acpi_state.madt =
        (acpi_madt_t *)acpi_find_table(ACPI_MADT_SIG);
    if (acpi_state.madt)
        printk("[acpi] MADT found at 0x%08x\n",
               (uint32_t)acpi_state.madt);

    acpi_state.initialized = 1;
    acpi_parse_madt();
    printk("[acpi] Initialization complete\n");
    return 0;
}

kscope_node_t acpi_node = {
    .name = "acpi",
    .id = 0x000A,
    .class = KSCOPE_CLASS_POWER,
    .subclass = KSCOPE_SUBCLASS_POWER_ACPI,
    .requires = (kscope_node_t *[]){&heap_node},
    .require_count = 1,
    .provides = (const char *[]){"power.acpi", "hw.tables"},
    .provide_count = 2,
    .init = acpi_init,
};

/* ==========================================================================
 * PM1a_CNT accessors
 * ======================================================================= */
static uint16_t acpi_read_pm1a_cnt(void) {
    if (!acpi_state.fadt || acpi_state.fadt->pm1a_cnt_blk == 0)
        return 0;
    return inw(acpi_state.fadt->pm1a_cnt_blk);
}

static void acpi_write_pm1a_cnt(uint16_t val) {
    if (!acpi_state.fadt || acpi_state.fadt->pm1a_cnt_blk == 0)
        return;
    outw(acpi_state.fadt->pm1a_cnt_blk, val);
}

/* ==========================================================================
 * Enable ACPI mode
 * ======================================================================= */
static void acpi_enable(void) {
    if (!acpi_state.fadt)
        return;

    if (acpi_state.fadt->pm1a_cnt_blk == 0) {
        printk("[acpi] No PM1a_CNT port, cannot enable\n");
        return;
    }

    if (acpi_read_pm1a_cnt() & ACPI_PM1_SCI_EN) {
        printk("[acpi] ACPI already enabled\n");
        return;
    }

    if (acpi_state.fadt->smi_cmd == 0) {
        printk("[acpi] WARNING: No SMI_CMD port\n");
        return;
    }

    printk("[acpi] Enabling ACPI...\n");
    outb(acpi_state.fadt->smi_cmd,
         acpi_state.fadt->acpi_enable);

    /* Wait up to ~3 seconds (QEMU is fast, real HW may need this) */
    for (int i = 0; i < 3000; i++) {
        if (acpi_read_pm1a_cnt() & ACPI_PM1_SCI_EN) {
            printk("[acpi] ACPI enabled successfully\n");
            return;
        }
        for (volatile int j = 0; j < 1000; j++);
    }

    printk("[acpi] WARNING: ACPI enable timed out\n");
}

/* ==========================================================================
 * Shutdown
 * Writes PM1a_CNT and PM1b_CNT (if present) with SLP_TYP + SLP_EN.
 * ======================================================================= */
void acpi_shutdown(void) {
    if (!acpi_state.initialized || !acpi_state.fadt) {
        printk("[acpi] ACPI not initialized, "
               "falling back to QEMU exit\n");
        outb(PORT_QEMU_EXIT, 0);
        return;
    }

    if (acpi_state.fadt->pm1a_cnt_blk == 0) {
        printk("[acpi] No PM1a_CNT port, "
               "falling back to QEMU exit\n");
        outb(PORT_QEMU_EXIT, 0);
        return;
    }

    acpi_enable();

    uint16_t pm1a_cnt = acpi_read_pm1a_cnt();
    uint16_t slp_typa = QEMU_SLP_TYPA;
    uint16_t shutdown_val =
        (pm1a_cnt & ~ACPI_PM1_SLP_TYP_MASK) |
        (slp_typa << ACPI_PM1_SLP_TYP_SHIFT) |
        ACPI_PM1_SLP_EN;

    printk("[acpi] Sending shutdown to PM1a_CNT (0x%04x)\n",
           acpi_state.fadt->pm1a_cnt_blk);
    acpi_write_pm1a_cnt(shutdown_val);

    /* PM1b_CNT: required by spec if present */
    if (acpi_state.fadt->pm1b_cnt_blk != 0) {
        uint16_t pm1b_cnt = inw(acpi_state.fadt->pm1b_cnt_blk);
        uint16_t slp_typb = QEMU_SLP_TYPB;
        uint16_t shutdown_val_b =
            (pm1b_cnt & ~ACPI_PM1_SLP_TYP_MASK) |
            (slp_typb << ACPI_PM1_SLP_TYP_SHIFT) |
            ACPI_PM1_SLP_EN;
        outw(acpi_state.fadt->pm1b_cnt_blk, shutdown_val_b);
    }

    /* Give hardware a moment, then halt forever */
    for (volatile int i = 0; i < 1000000; i++);
    printk("[acpi] Shutdown did not complete, halting\n");
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

/* ==========================================================================
 * Reboot
 * Tries ACPI reset register, then keyboard controller, then triple fault.
 * ======================================================================= */
void acpi_reboot(void) {
    size_t reset_end = offsetof(acpi_fadt_t, reset_reg) +
                       sizeof(acpi_gas_t) + sizeof(uint8_t);

    if (acpi_state.initialized && acpi_state.fadt &&
        acpi_state.fadt->header.length >= reset_end &&
        acpi_state.fadt->reset_reg.address != 0) {

        printk("[acpi] Using ACPI reset register\n");

        if (acpi_state.fadt->reset_reg.address_space_id == 0) {
            /* System I/O space */
            outb((uint16_t)acpi_state.fadt->reset_reg.address,
                 acpi_state.fadt->reset_value);
        } else if (acpi_state.fadt->reset_reg.address_space_id == 1) {
            /* System memory space */
            volatile uint8_t *reg =
                (volatile uint8_t *)auto_virt(
                    (uint32_t)acpi_state.fadt->reset_reg.address);
            *reg = acpi_state.fadt->reset_value;
        }

        /* Brief delay for hardware to react */
        for (volatile int i = 0; i < 1000000; i++);
    }

    printk("[acpi] Falling back to keyboard controller reset\n");
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);

    printk("[acpi] Reset failed, triple faulting...\n");
    __asm__ __volatile__("int $0xFF");
}

/* ==========================================================================
 * Accessors
 * ======================================================================= */
acpi_fadt_t *acpi_get_fadt(void) {
    return acpi_state.fadt;
}

acpi_madt_t *acpi_get_madt(void) {
    return acpi_state.madt;
}

int acpi_is_initialized(void) {
    return acpi_state.initialized;
}

/* ==========================================================================
 * Debug output
 * ======================================================================= */
void acpi_print_info(void) {
    if (!acpi_state.initialized) {
        printk("[acpi] Not initialized\n");
        return;
    }

    printk("\n=== ACPI Information ===\n");
    printk("RSDP at:     0x%08x\n", (uint32_t)acpi_state.rsdp);
    printk("ACPI ver:    %d.0\n", acpi_state.acpi_version);
    printk("FADT at:     0x%08x\n", (uint32_t)acpi_state.fadt);

    if (acpi_state.fadt) {
        printk("  DSDT:      0x%08x\n", acpi_state.fadt->dsdt);
        printk("  SMI_CMD:   0x%04x\n", acpi_state.fadt->smi_cmd);
        printk("  PM1a_CNT:  0x%04x\n",
               acpi_state.fadt->pm1a_cnt_blk);
        printk("  PM1b_CNT:  0x%04x\n",
               acpi_state.fadt->pm1b_cnt_blk);
        printk("  ACPI_EN:   0x%02x\n",
               acpi_state.fadt->acpi_enable);
        printk("  ACPI_DIS:  0x%02x\n",
               acpi_state.fadt->acpi_disable);
    }

    if (acpi_state.madt) {
        printk("MADT at:     0x%08x\n", (uint32_t)acpi_state.madt);
        printk("  LAPIC:     0x%08x\n",
               acpi_state.madt->local_apic_addr);
    }

    printk("========================\n\n");
}

/* ==========================================================================
 * MADT parser
 * ======================================================================= */
void acpi_parse_madt(void) {
    if (!acpi_state.madt) {
        printk("[acpi] No MADT to parse\n");
        return;
    }

    memset(&madt_parsed, 0, sizeof(madt_parsed));
    for (int i = 0; i < 16; i++)
        madt_parsed.iso_map[i] = i;

    uint32_t madt_len = acpi_state.madt->header.length;
    uint32_t offset = sizeof(acpi_madt_t);

    while (offset + sizeof(madt_entry_header_t) <= madt_len) {
        madt_entry_header_t *entry =
            (madt_entry_header_t *)((uint8_t *)acpi_state.madt +
                                    offset);

        if (entry->length < sizeof(madt_entry_header_t) ||
            offset + entry->length > madt_len) {
            printk("[acpi] MADT bad length %u at offset %u, "
                   "aborting\n", entry->length, offset);
            break;
        }

        switch (entry->type) {
        case MADT_TYPE_LAPIC: {
            madt_lapic_t *lapic = (madt_lapic_t *)entry;
            if (madt_parsed.num_cpus < 8) {
                uint8_t idx = madt_parsed.num_cpus++;
                madt_parsed.cpu_apic_ids[idx] = lapic->apic_id;
                madt_parsed.cpu_enabled[idx] =
                    (lapic->flags & 1) ? 1 : 0;
            }
            break;
        }

        case MADT_TYPE_IOAPIC: {
            madt_ioapic_t *ioapic = (madt_ioapic_t *)entry;
            madt_parsed.has_ioapic = 1;
            madt_parsed.ioapic_addr = ioapic->io_apic_addr;
            madt_parsed.ioapic_gsi_base = ioapic->gsi_base;
            break;
        }

        case MADT_TYPE_ISO: {
            madt_iso_t *iso = (madt_iso_t *)entry;
            if (iso->irq_source < 16)
                madt_parsed.iso_map[iso->irq_source] =
                    (int)iso->gsi;
            break;
        }

        case MADT_TYPE_LAPIC_ADDR:
            /* 64-bit local APIC override — ignored on 32-bit */
            break;

        default:
            /* Unknown type, skip silently */
            break;
        }

        offset += entry->length;
    }

    printk("[acpi] MADT parsed: %u CPU(s), I/O APIC %s\n",
           madt_parsed.num_cpus,
           madt_parsed.has_ioapic ? "present" : "absent");
    for (uint8_t i = 0; i < madt_parsed.num_cpus; i++) {
        printk("  CPU %u: APIC ID=%u, %s\n",
               i, madt_parsed.cpu_apic_ids[i],
               madt_parsed.cpu_enabled[i] ? "enabled" : "disabled");
    }
    if (madt_parsed.has_ioapic) {
        printk("  I/O APIC at 0x%08x, GSI base %u\n",
               madt_parsed.ioapic_addr,
               madt_parsed.ioapic_gsi_base);
    }
}

/* ==========================================================================
 * Parsed MADT accessor
 * ======================================================================= */
const madt_parsed_t* acpi_get_madt_parsed(void) {
    return &madt_parsed;
}