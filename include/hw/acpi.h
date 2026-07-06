/*
	* hw/acpi.h - ACPI transparency layer (backed by ACPICA)
	* Author:   amity
	* Date:     Thu Jun 11 13:12:55 2026
	* Copyright © 2026 OwlyNest
*/

#ifndef __HW_ACPI_H__
#define __HW_ACPI_H__

/* --- Includes ---*/
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ACPI PM1 control bits — legacy fallback path only
 * ======================================================================= */
#define ACPI_PM1_SLP_EN         (1 << 13)
#define ACPI_PM1_SLP_TYP_MASK   0x1C00
#define ACPI_PM1_SLP_TYP_SHIFT  10

/* ==========================================================================
 * QEMU/VBox hardcoded SLP_TYP values — legacy fallback path only
 * ======================================================================= */
#define QEMU_SLP_TYPA           0x0001
#define QEMU_SLP_TYPB           0x2001

/* ==========================================================================
 * MADT entry types
 * ======================================================================= */
#define MADT_TYPE_LAPIC         0
#define MADT_TYPE_IOAPIC        1
#define MADT_TYPE_ISO           2
#define MADT_TYPE_NMI           4
#define MADT_TYPE_LAPIC_ADDR    5

/* --- Typedefs - Structs - Enums ---*/

/* ==========================================================================
 * Generic Address Structure (GAS) — matches spec layout, overlays
 * directly onto whatever ACPICA hands back
 * ======================================================================= */
typedef struct __attribute__((packed)) {
    uint8_t  address_space_id;
    uint8_t  bit_width;
    uint8_t  bit_offset;
    uint8_t  access_size;
    uint64_t address;
} acpi_gas_t;

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
 * FADT — only the fields this kernel actually reads
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

    acpi_gas_t reset_reg;
    uint8_t    reset_value;
    uint16_t   arm_boot_arch;
    uint8_t    fadt_minor_version;

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
 * MADT — header + variable entries, walked by acpi_parse_madt
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

typedef struct __attribute__((packed)) {
    madt_entry_header_t header;
    uint8_t  acpi_processor_id;
    uint8_t  apic_id;
    uint32_t flags;
} madt_lapic_t;

typedef struct __attribute__((packed)) {
    madt_entry_header_t header;
    uint8_t  io_apic_id;
    uint8_t  reserved;
    uint32_t io_apic_addr;
    uint32_t gsi_base;
} madt_ioapic_t;

typedef struct __attribute__((packed)) {
    madt_entry_header_t header;
    uint8_t  bus_source;
    uint8_t  irq_source;
    uint32_t gsi;
    uint16_t flags;
} madt_iso_t;

/* Parsed MADT summary — the friendly shape ACPICA doesn't hand you */
typedef struct {
    uint8_t  num_cpus;
    uint8_t  cpu_apic_ids[8];
    uint8_t  cpu_enabled[8];

    int      has_ioapic;
    uint32_t ioapic_addr;
    uint32_t ioapic_gsi_base;

    int      iso_map[16];      /* isa_irq -> gsi, identity if unmapped */
} madt_parsed_t;

/* --- Prototypes ---*/

/* ==========================================================================
 * Power management — the only two calls the rest of the kernel needs
 * ======================================================================= */
void acpi_shutdown(void);
void acpi_reboot(void);

/* ==========================================================================
 * Table access — thin views over ACPICA's already-validated tables
 * ======================================================================= */
acpi_fadt_t *acpi_get_fadt(void);
acpi_madt_t *acpi_get_madt(void);
void *acpi_find_table(const char *signature);
int  acpi_is_initialized(void);

/* ==========================================================================
 * MADT summary
 * ======================================================================= */
void acpi_parse_madt(void);
const madt_parsed_t *acpi_get_madt_parsed(void);

/* ==========================================================================
 * Debug output
 * ======================================================================= */
void acpi_print_info(void);

#endif