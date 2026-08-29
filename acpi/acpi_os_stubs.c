#include "acpi.h"
#include <acexcep.h>
#include <arch/x86/io.h>
#include <arch/x86/interrupts.h>
#include <screen/printk.h>
#include <hw/pci.h>

static ACPI_OSD_HANDLER acpi_handler = NULL;
static void *acpi_context = NULL;
static uint32_t sci_storm_count = 0;

static int acpi_irq_wrapper(interrupt_frame_t *frame) {
    (void)frame;

    ACPI_TABLE_FADT *fadt = NULL;
    if (ACPI_FAILURE(AcpiGetTable(ACPI_SIG_FADT, 1, (ACPI_TABLE_HEADER **)&fadt))) {
        printk("[acpi] Failed to get FADT\n");
        return 1;
    }

    uint16_t pm1_sts = inw(fadt->Pm1aEventBlock);

    if (pm1_sts == 0) {
        return 0;  /* Not our interrupt */
    }
    
    UINT32 handled = ACPI_INTERRUPT_NOT_HANDLED;
    if (acpi_handler) {
        handled = acpi_handler(acpi_context);
    }

    if (handled == ACPI_INTERRUPT_HANDLED) {
        sci_storm_count = 0;
    } else {
        if (++sci_storm_count > 100) {
            pic_mask_irq(9);
            printk("[OSL] SCI IRQ storm on IRQ9, masked\n");
        }
    }

    return 1;
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context) {
    acpi_handler = ServiceRoutine;
    acpi_context = Context;

    if (InterruptNumber != 9) {
        return AE_SUPPORT;
    }

    register_interrupt_handler(32 + InterruptNumber, acpi_irq_wrapper);

    pic_unmask_irq(InterruptNumber);

    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine) {
    (void)ServiceRoutine;
    acpi_handler = NULL;
    acpi_context = NULL;

    register_interrupt_handler(32 + InterruptNumber, NULL);

    return AE_OK;
}

void AcpiOsWaitEventsComplete(void) {

}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width) {
    if (PciId->Segment != 0) {
        return AE_SUPPORT;
    }
    uint8_t bus  = (uint8_t)PciId->Bus;
    uint8_t dev  = (uint8_t)PciId->Device;
    uint8_t func = (uint8_t)PciId->Function;
    switch (Width) {
        case 8:
            *Value = pci_read_config_byte(bus,dev, func, Reg);
            break;
        case 16:
            *Value = pci_read_config_word(bus,dev, func, Reg);
            break;
        case 32:
            *Value = pci_read_config(bus,dev, func, Reg);
            break;
        case 64: {
            uint64_t lo = pci_read_config(bus,dev, func, Reg);
            uint64_t hi = pci_read_config(bus,dev, func, Reg + 4);
            *Value = lo | (hi << 32);
            break;
        }
        default:
            return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width) {
    if (PciId->Segment != 0) {
        return AE_SUPPORT;
    }
    uint8_t bus  = (uint8_t)PciId->Bus;
    uint8_t dev  = (uint8_t)PciId->Device;
    uint8_t func = (uint8_t)PciId->Function;
    if ((Width == 16 && (Reg & 1)) || (Width == 32 && (Reg & 3)) || (Width == 64 && (Reg & 7))) {
        return AE_BAD_PARAMETER;
    }

    switch (Width) {
        case 8:
            pci_write_config_byte(bus, dev, func, Reg, (uint8_t)Value);
            break;
        case 16:
            pci_write_config_word(bus, dev, func, Reg, (uint16_t)Value);
            break;
        case 32:
            pci_write_config(bus, dev, func, Reg, Value);
            break;
        case 64:
            pci_write_config(bus, dev, func, Reg, Value);
            pci_write_config(bus, dev, func, Reg + 4, (Value >> 32));
            break;
        default:
            return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegA, UINT32 RegB) {
    (void)SleepState; (void)RegA; (void)RegB;
    return AE_OK;
}