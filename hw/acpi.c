/*
 * hw/acpi.c - ACPI transparency layer (backed by ACPICA)
 * Author:   amity
 * Date:     Thu Jun 11 13:12:53 2026
 * Copyright © 2026 OwlyNest
 */

/* --- Includes ---*/
#include "acpi.h" /* ACPICA — kept out of hw/acpi.h */
#include <arch/x86/io.h>
#include <hw/acpi.h>
#include <internal/phonon_consts.h>
#include <lib/string.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Globals ---*/
static madt_parsed_t madt_parsed = {0};

/* --- Prototypes ---*/
static void acpi_legacy_shutdown(void);
static void acpi_legacy_reboot(void);

/* --- Functions ---*/

/* ==========================================================================
 * Shutdown — ACPICA path first (S5), hand-rolled PM1 write as fallback
 * ======================================================================= */
void acpi_shutdown(void) {
  ACPI_STATUS status;

  status = AcpiEnterSleepStatePrep(ACPI_STATE_S5);
  if (ACPI_SUCCESS(status)) {
    printk("[acpi] Entering S5 via ACPICA\n");

    asm volatile("cli"); /* required before the final write */
    status = AcpiEnterSleepState(ACPI_STATE_S5);

    if (ACPI_FAILURE(status)) {
      printk("[acpi] AcpiEnterSleepState failed: %u\n", status);
    }
  } else {
    printk("[acpi] AcpiEnterSleepStatePrep failed: %u\n", status);
  }

  /* Only reached if the ACPICA path didn't actually power us off */
  printk("[acpi] Falling back to legacy shutdown\n");
  acpi_legacy_shutdown();
}

/* ==========================================================================
 * Reboot — ACPICA reset register first, hardware fallbacks after
 * ======================================================================= */
void acpi_reboot(void) {
  ACPI_STATUS status = AcpiReset();

  if (ACPI_SUCCESS(status)) {
    printk("[acpi] Reset requested via ACPICA\n");
    for (volatile int i = 0; i < 1000000; i++)
      ;
  } else {
    printk("[acpi] AcpiReset failed: %u\n", status);
  }

  printk("[acpi] Falling back to legacy reset\n");
  acpi_legacy_reboot();
}

/* ==========================================================================
 * Legacy fallback: raw PM1a/PM1b_CNT write, same as the pre-ACPICA days
 * ======================================================================= */
static void acpi_legacy_shutdown(void) {
  acpi_fadt_t *fadt = acpi_get_fadt();

  if (!fadt || fadt->pm1a_cnt_blk == 0) {
    printk("[acpi] No usable PM1a_CNT, falling back to QEMU exit\n");
    outb(PORT_QEMU_EXIT, 0);
    return;
  }

  uint16_t pm1a_cnt = inw(fadt->pm1a_cnt_blk);
  uint16_t shutdown_val = (pm1a_cnt & ~ACPI_PM1_SLP_TYP_MASK) |
                          (QEMU_SLP_TYPA << ACPI_PM1_SLP_TYP_SHIFT) |
                          ACPI_PM1_SLP_EN;

  printk("[acpi] Legacy shutdown via PM1a_CNT (0x%04x)\n", fadt->pm1a_cnt_blk);
  outw(fadt->pm1a_cnt_blk, shutdown_val);

  if (fadt->pm1b_cnt_blk != 0) {
    uint16_t pm1b_cnt = inw(fadt->pm1b_cnt_blk);
    uint16_t shutdown_val_b = (pm1b_cnt & ~ACPI_PM1_SLP_TYP_MASK) |
                              (QEMU_SLP_TYPB << ACPI_PM1_SLP_TYP_SHIFT) |
                              ACPI_PM1_SLP_EN;
    outw(fadt->pm1b_cnt_blk, shutdown_val_b);
  }

  for (volatile int i = 0; i < 1000000; i++)
    ;
  printk("[acpi] Shutdown did not complete, halting\n");
  for (;;) {
    asm volatile("cli; hlt");
  }
}

/* ==========================================================================
 * Legacy fallback: keyboard controller reset, then triple fault
 * ======================================================================= */
static void acpi_legacy_reboot(void) {
  uint8_t good = 0x02;

  printk("[acpi] Trying keyboard controller reset\n");
  while (good & 0x02)
    good = inb(0x64);
  outb(0x64, 0xFE);

  for (volatile int i = 0; i < 1000000; i++)
    ;

  printk("[acpi] Reset failed, triple faulting...\n");
  asm volatile("int $0xFF");
}

/* ==========================================================================
 * Table accessors — always asked fresh, never cached
 * ======================================================================= */
acpi_fadt_t *acpi_get_fadt(void) {
  ACPI_TABLE_HEADER *table;

  if (ACPI_FAILURE(AcpiGetTable(ACPI_SIG_FADT, 1, &table))) {
    return NULL;
  }

  return (acpi_fadt_t *)table;
}

acpi_madt_t *acpi_get_madt(void) {
  ACPI_TABLE_HEADER *table;

  if (ACPI_FAILURE(AcpiGetTable(ACPI_SIG_MADT, 1, &table))) {
    return NULL;
  }

  return (acpi_madt_t *)table;
}

void *acpi_find_table(const char *signature) {
  ACPI_TABLE_HEADER *table;

  if (ACPI_FAILURE(AcpiGetTable((ACPI_STRING)signature, 1, &table))) {
    return NULL;
  }

  return (void *)table;
}

int acpi_is_initialized(void) {
  ACPI_TABLE_HEADER *table;

  return ACPI_SUCCESS(AcpiGetTable(ACPI_SIG_DSDT, 1, &table));
}

/* ==========================================================================
 * Debug output
 * ======================================================================= */
void acpi_print_info(void) {
  acpi_fadt_t *fadt = acpi_get_fadt();
  if (fadt) {
    printk("[acpi] fadt ptr=0x%p len=%u sig=%02x %02x %02x %02x\n", fadt,
           fadt->header.length, ((uint8_t *)fadt)[0], ((uint8_t *)fadt)[1],
           ((uint8_t *)fadt)[2], ((uint8_t *)fadt)[3]);
  }
  acpi_madt_t *madt = acpi_get_madt();
  acpi_parse_madt();

  if (!fadt) {
    printk("[acpi] Not initialized (no FADT available)\n");
    return;
  }

  printk("\n=== ACPI Information (via ACPICA) ===\n");
  printk("FADT at:     0x%08x (length %u, rev %u)\n", (uint32_t)fadt,
         fadt->header.length, fadt->header.revision);
  printk("  Signature: %4.4s\n", fadt->header.signature);
  printk("  DSDT:      0x%08x\n", fadt->dsdt);
  printk("  SMI_CMD:   0x%04x\n", fadt->smi_cmd);
  printk("  PM1a_CNT:  0x%04x\n", fadt->pm1a_cnt_blk);
  printk("  PM1b_CNT:  0x%04x\n", fadt->pm1b_cnt_blk);
  printk("  ACPI_EN:   0x%02x\n", fadt->acpi_enable);
  printk("  ACPI_DIS:  0x%02x\n", fadt->acpi_disable);

  if (fadt->header.length >=
      offsetof(acpi_fadt_t, reset_value) + sizeof(fadt->reset_value)) {
    printk("  Reset reg: space=%u addr=0x%016llx val=0x%02x\n",
           fadt->reset_reg.address_space_id,
           (unsigned long long)fadt->reset_reg.address, fadt->reset_value);
  } else {
    printk("  Reset reg: not present (ACPI 1.0 table)\n");
  }

  if (madt) {
    printk("MADT at:     0x%08x\n", (uint32_t)madt);
    printk("  Cores      0x%08x\n", (uint32_t)madt_parsed.num_cpus);
    printk("  LAPIC:     0x%08x\n", madt->local_apic_addr);
  } else {
    printk("MADT:        not present\n");
  }

  printk("======================================\n\n");
}

/* ==========================================================================
 * MADT parser — ACPICA doesn't expose this summary shape, so it stays
 * ======================================================================= */
void acpi_parse_madt(void) {
  acpi_madt_t *madt = acpi_get_madt();

  if (!madt) {
    printk("[acpi] No MADT to parse\n");
    return;
  }

  memset(&madt_parsed, 0, sizeof(madt_parsed));
  for (int i = 0; i < 16; i++) {
    madt_parsed.iso_map[i] = i;
  }

  uint32_t madt_len = madt->header.length;
  uint32_t offset = sizeof(acpi_madt_t);

  while (offset + sizeof(madt_entry_header_t) <= madt_len) {
    madt_entry_header_t *entry =
        (madt_entry_header_t *)((uint8_t *)madt + offset);

    if (entry->length < sizeof(madt_entry_header_t) ||
        offset + entry->length > madt_len) {
      printk("[acpi] MADT bad length %u at offset %u, "
             "aborting\n",
             entry->length, offset);
      break;
    }

    switch (entry->type) {
    case MADT_TYPE_LAPIC: {
      madt_lapic_t *lapic = (madt_lapic_t *)entry;
      if (madt_parsed.num_cpus < 8) {
        uint8_t idx = madt_parsed.num_cpus++;
        madt_parsed.cpu_apic_ids[idx] = lapic->apic_id;
        madt_parsed.cpu_enabled[idx] = (lapic->flags & 1) ? 1 : 0;
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
      if (iso->irq_source < 16) {
        madt_parsed.iso_map[iso->irq_source] = (int)iso->gsi;
      }
      break;
    }

    case MADT_TYPE_LAPIC_ADDR:
      break;

    default:
      break;
    }

    offset += entry->length;
  }

  printk("[acpi] MADT parsed: %u CPU(s), I/O APIC %s\n", madt_parsed.num_cpus,
         madt_parsed.has_ioapic ? "present" : "absent");
  for (uint8_t i = 0; i < madt_parsed.num_cpus; i++) {
    printk("  CPU %u: APIC ID=%u, %s\n", i, madt_parsed.cpu_apic_ids[i],
           madt_parsed.cpu_enabled[i] ? "enabled" : "disabled");
  }
  if (madt_parsed.has_ioapic) {
    printk("  I/O APIC at 0x%08x, GSI base %u\n", madt_parsed.ioapic_addr,
           madt_parsed.ioapic_gsi_base);
  }
}

const madt_parsed_t *acpi_get_madt_parsed(void) { return &madt_parsed; }