#include "acpi.h"

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context) {
    (void)InterruptNumber; (void)ServiceRoutine; (void)Context;
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine) {
    (void)InterruptNumber; (void)ServiceRoutine;
    return AE_OK;
}

void AcpiOsWaitEventsComplete(void) {

}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width) {
    (void)PciId; (void)Reg; (void)Value; (void)Width;
    return AE_SUPPORT;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width) {
    (void)PciId; (void)Reg; (void)Value; (void)Width;
    return AE_SUPPORT;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegA, UINT32 RegB) {
    (void)SleepState; (void)RegA; (void)RegB;
    return AE_OK;
}