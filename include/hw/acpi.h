/*
	* hw/acpi.h - [Enter description]
	* Author:   amity
	* Date:     Thu Jun 11 13:12:55 2026
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

#ifndef ACPI_H
#define ACPI_H
/* --- Includes ---*/
#include <stddef.h>
#include <stdint.h>
/* --- Macros ---*/
/* ==========================================================================
 * Signatures
 * ======================================================================= */
#define ACPI_RSDP_SIG       "RSD PTR "
#define ACPI_RSDP_SIG_LEN   8
#define ACPI_RSDT_SIG       "RSDT"
#define ACPI_XSDT_SIG       "XSDT"
#define ACPI_FADT_SIG       "FACP"
#define ACPI_MADT_SIG       "APIC"
#define ACPI_DSDT_SIG       "DSDT"
#define ACPI_SSDT_SIG       "SSDT"
/* ==========================================================================
* MADT
* ======================================================================= */
#define MADT_TYPE_LAPIC 0
#define MADT_TYPE_IOAPIC 1
#define MADT_TYPE_ISO 2
#define MADT_TYPE_NMI 4
#define MADT_TYPE_LAPIC_ADDR 5

/* ==========================================================================
 * ACPI PM1 control bits
 * ======================================================================= */
#define ACPI_PM1_SLP_EN     (1 << 13)
#define ACPI_PM1_SLP_TYP_MASK   0x1C00
#define ACPI_PM1_SLP_TYP_SHIFT  10
#define ACPI_PM1_SCI_EN     (1 << 0)


/* ==========================================================================
 * QEMU PIIX4 hardcoded SLP_TYP values (for systems without AML parser)
 * ======================================================================= */
#define QEMU_SLP_TYPA       0x0001
#define QEMU_SLP_TYPB       0x2001

/* --- Typedefs - Structs - Enums ---*/
/* ==========================================================================
 * RSDP — Root System Description Pointer
 * ======================================================================= */
 
typedef struct __attribute__((packed)) {
	char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_addr;
    /* ACPI 2.0+ */
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} acpi_rsdp_t;

/* ==========================================================================
 * SDT Header — common to all ACPI tables
 * ======================================================================= */
typedef struct __attribute__((packed)) {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_header_t;

/* ==========================================================================
 * RSDT — Root System Description Table (32-bit pointers)
 * ======================================================================= */
typedef struct __attribute__((packed)) {
    acpi_sdt_header_t header;
    uint32_t entries[0];  /* Variable length */
} acpi_rsdt_t;

/* ==========================================================================
 * XSDT — Extended System Description Table (64-bit pointers)
 * ======================================================================= */
typedef struct __attribute__((packed)) {
    acpi_sdt_header_t header;
    uint64_t entries[0];  /* Variable length */
} acpi_xsdt_t;


/* ==========================================================================
 * Generic Address Structure (GAS)
 * ======================================================================= */
typedef struct __attribute__((packed)) {
    uint8_t  address_space_id;
    uint8_t  bit_width;
    uint8_t  bit_offset;
    uint8_t  access_size;
    uint64_t address;
} acpi_gas_t;

/* ==========================================================================
 * FADT — Fixed ACPI Description Table
 * ======================================================================= */
typedef struct __attribute__((packed)) {
    acpi_sdt_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;

    uint8_t  reserved0;
    uint8_t  preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_cnt;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t  pm1_evt_len;
    uint8_t  pm1_cnt_len;
    uint8_t  pm2_cnt_len;
    uint8_t  pm_tmr_len;
    uint8_t  gpe0_blk_len;
    uint8_t  gpe1_blk_len;
    uint8_t  gpe1_base;
    uint8_t  cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alrm;
    uint8_t  mon_alrm;
    uint8_t  century;
    uint16_t iapc_boot_arch;
    uint8_t  reserved1;
    uint32_t flags;

    /* ACPI 2.0+ fields */
    acpi_gas_t reset_reg;
    uint8_t    reset_value;
    uint16_t   arm_boot_arch;
    uint8_t    fadt_minor_version;

    /* ACPI 5.0+ fields */
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
    acpi_gas_t x_pm1a_evt_blk;
    acpi_gas_t x_pm1b_evt_blk;
    acpi_gas_t x_pm1a_cnt_blk;
    acpi_gas_t x_pm1b_cnt_blk;
    acpi_gas_t x_pm2_cnt_blk;
    acpi_gas_t x_pm_tmr_blk;
    acpi_gas_t x_gpe0_blk;
    acpi_gas_t x_gpe1_blk;
    acpi_gas_t sleep_control_reg;
    acpi_gas_t sleep_status_reg;
    uint64_t   hypervisor_vendor_identity;
} acpi_fadt_t;

/* ==========================================================================
 * MADT — Multiple APIC Description Table (for future SMP)
 * ======================================================================= */
typedef struct __attribute__((packed)) {
    acpi_sdt_header_t header;
    uint32_t local_apic_addr;
    uint32_t flags;
    uint8_t  entries[0];
} acpi_madt_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
} madt_entry_header_t;

/* Type 0: Processor Local APIC */
typedef struct __attribute__((packed)) {
    madt_entry_header_t header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags; /* bit 0: enabled, bit 1: online capable */
} madt_lapic_t;

/* Type 1: I/O APIC*/
typedef struct __attribute__((packed)) {
    madt_entry_header_t header;
    uint8_t  io_apic_id;
    uint8_t  reserved;
    uint32_t io_apic_addr;
    uint32_t gsi_base;
} madt_ioapic_t;

/* Type 2: Interrupt Source Override */
typedef struct __attribute__((packed)) {
    madt_entry_header_t header;
    uint8_t  bus_source;      /* always 0 (ISA) */
    uint8_t  irq_source;      /* ISA IRQ (0-15) */
    uint32_t gsi;             /* Global System Interrupt */
    uint16_t flags;           /* polarity, trigger mode */
} madt_iso_t;

/* Parsed MADT state */
typedef struct {
    uint8_t  num_cpus;
    uint8_t  cpu_apic_ids[8];     /* Up to 8 cores for now */
    uint8_t  cpu_enabled[8];

    int      has_ioapic;
    uint32_t ioapic_addr;
    uint32_t ioapic_gsi_base;

    /* IRQ remapping: isa_irq[0-15] -> gsi, -1 if not overridden */
    int      iso_map[16];
} madt_parsed_t;

/* ==========================================================================
 * Runtime state
 * ======================================================================= */
typedef struct {
    int initialized;
    acpi_rsdp_t* rsdp;
    acpi_fadt_t* fadt;
    acpi_madt_t* madt;
    uint8_t acpi_version;  /* 1 or 2+ */
} acpi_state_t;

/* --- Globals ---*/

/* --- Prototypes ---*/
/* Initialization */
void acpi_print_info(void);

/* Power management */
void acpi_shutdown(void);
void acpi_reboot(void);

/* Table access */
acpi_fadt_t* acpi_get_fadt(void);
acpi_madt_t* acpi_get_madt(void);
int  acpi_is_initialized(void);

/* Low-level helpers (exposed for debugging) */
void* acpi_find_table(const char* signature);

void acpi_parse_madt(void);
const madt_parsed_t* acpi_get_madt_parsed(void);
#endif