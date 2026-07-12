/*
	* acpi/acpi_init.c - [Enter description]
	* Author:   amity
	* Date:     Sun Jul  5 15:25:11 2026
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
#include "acpi.h"
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <screen/printk.h>
/* --- Includes ---*/

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
void acpi_print_pm1_ports(void) {
    ACPI_TABLE_FADT *fadt = NULL;
    if (ACPI_FAILURE(AcpiGetTable(ACPI_SIG_FADT, 1, (ACPI_TABLE_HEADER **)&fadt))) {
        printk("[acpi] Failed to get FADT\n");
        return;
    }

    printk("[acpi] PM1a_EVT_BLK = 0x%08x (len %u)\n",
           fadt->Pm1aEventBlock, fadt->Pm1EventLength);
    printk("[acpi] PM1b_EVT_BLK = 0x%08x\n", fadt->Pm1bEventBlock);
    printk("[acpi] PM1a_CNT_BLK  = 0x%08x\n", fadt->Pm1aControlBlock);
    printk("[acpi] PM1b_CNT_BLK  = 0x%08x\n", fadt->Pm1bControlBlock);

    /* ACPI 2.0+ extended addresses */
    printk("[acpi] X_PM1a_EVT: space=%u addr=0x%llx width=%u\n",
           fadt->XPm1aEventBlock.SpaceId,
           fadt->XPm1aEventBlock.Address,
           fadt->XPm1aEventBlock.BitWidth);
}

static int acpi_subsystem_init(void) {
    ACPI_STATUS status;

    status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(status)) {
        printk("[acpi] AcpiInitializeSubsystem failed: %u\n", status);
        return -1;
    }

    status = AcpiInitializeTables(NULL, 16, FALSE);
    if (ACPI_FAILURE(status)) {
        printk("[acpi] AcpiInitializeTables failed: %u\n", status);
        return -1;
    }

    status = AcpiLoadTables();
    if (ACPI_FAILURE(status)) {
        printk("[acpi] AcpiLoadTables failed: %u\n", status);
        return -1;
    }
    
    acpi_print_pm1_ports();

    status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(status)) {
        printk("[acpi] AcpiEnableSubsystem failed: %u\n", status);
        return -1;
    }

    status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(status)) {
        printk("[acpi] AcpiInitializeObjects failed: %u\n", status);
        return -1;
    }

    printk("[acpi] ACPICA initialized\n");
    return 0;
}

kscope_node_t acpi_node = {
    .name = "acpi",
    .id = 0x000A,
    .class = KSCOPE_CLASS_POWER,
    .subclass = KSCOPE_SUBCLASS_POWER_ACPI,
    .requires = (kscope_node_t *[]){&heap_node, &scheduler_node, &x86_idt_node, &paging_node, &pci_node},
    .require_count = 5,
    .provides = (const char *[]){"power.acpi", "hw.tables"},
    .provide_count = 2,
    .init = acpi_subsystem_init,
};