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
    .requires = (kscope_node_t *[]){&heap_node, &scheduler_node, &x86_idt_node, &paging_node},
    .require_count = 4,
    .provides = (const char *[]){"power.acpi", "hw.tables"},
    .provide_count = 2,
    .init = acpi_subsystem_init,
};