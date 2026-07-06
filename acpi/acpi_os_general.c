/*
	* acpi/acpi_os_general.c - [Enter description]
	* Author:   amity
	* Date:     Sun Jul  5 15:23:10 2026
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
#include "acpi.h"
#include <mm/heap.h>
#include <arch/x86/io.h>
#include <internal/virtmem.h>
#include <screen/printk.h>
#include <mm/vmm.h>
#include <mm/paging.h>
#include <stdarg.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/
ACPI_STATUS AcpiOsInitialize(void)  { return AE_OK; }
ACPI_STATUS AcpiOsTerminate(void)   { return AE_OK; }

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void) {
    ACPI_PHYSICAL_ADDRESS addr = 0;
    AcpiFindRootPointer(&addr);        /* ACPICA does the EBDA/BIOS scan */
    return addr;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *Predefined, ACPI_STRING *NewValue) {
    (void)Predefined;
    *NewValue = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable) {
    (void)ExistingTable;
    *NewTable = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewLength) {
    (void)ExistingTable;
    *NewAddress = 0;
    *NewLength = 0;
    return AE_OK;
}

void *AcpiOsAllocate(ACPI_SIZE Size)          {
    return malloc(Size);
}

void  AcpiOsFree(void *Memory) {
    free(Memory);
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length) {
    return vmm_map_physical((uintptr_t)PhysicalAddress, (size_t)Length, PAGE_WRITABLE);
}

void AcpiOsUnmapMemory(void *where, ACPI_SIZE length) {
    vmm_unmap_physical(where, (size_t)length);
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width) {
    size_t width_bytes = Width / 8;
    void *v = vmm_map_physical((uintptr_t)Address, width_bytes, PAGE_WRITABLE);
    if (!v) return AE_NO_MEMORY;

    switch (Width) {
        case 8:  *Value = *(volatile uint8_t *)v;  break;
        case 16: *Value = *(volatile uint16_t *)v; break;
        case 32: *Value = *(volatile uint32_t *)v; break;
        case 64: *Value = *(volatile uint64_t *)v; break;
        default: vmm_unmap_physical(v, width_bytes); return AE_BAD_PARAMETER;
    }

    vmm_unmap_physical(v, width_bytes);
    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width) {
    size_t width_bytes = Width / 8;
    void *v = vmm_map_physical((uintptr_t)Address, width_bytes, PAGE_WRITABLE);
    if (!v) return AE_NO_MEMORY;

    switch (Width) {
        case 8:  *(volatile uint8_t *)v = Value;  break;
        case 16: *(volatile uint16_t *)v = Value; break;
        case 32: *(volatile uint32_t *)v = Value; break;
        case 64: *(volatile uint64_t *)v = Value; break;
        default: vmm_unmap_physical(v, width_bytes); return AE_BAD_PARAMETER;
    }

    vmm_unmap_physical(v, width_bytes);
    return AE_OK;
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width) {
    switch (Width) {
    case 8:  *Value = inb((uint16_t)Address);  break;
    case 16: *Value = inw((uint16_t)Address);  break;
    case 32: *Value = inl((uint16_t)Address);  break;
    default: return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
    switch (Width) {
    case 8:  outb((uint16_t)Address, (uint8_t)Value);  break;
    case 16: outw((uint16_t)Address, (uint16_t)Value); break;
    case 32: outl((uint16_t)Address, Value);           break;
    default: return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

void AcpiOsVprintf(const char *Format, va_list Args) {
    char buf[256];
    kvsnprintf(buf, sizeof(buf), Format, Args);
    printk("%s", buf);
}

void AcpiOsPrintf(const char *Format, ...) {
    va_list args;
    va_start(args, Format);
    AcpiOsVprintf(Format, args);
    va_end(args);
}

UINT64 AcpiOsGetTimer(void) {
    extern volatile uint32_t tick_count;
    return (UINT64)tick_count * 10000ULL;  /* ms -> 100ns units, approx */
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info) {
    switch (Function) {
    case ACPI_SIGNAL_FATAL:
        printk("[acpi] FATAL signal from ACPICA\n");
        for (;;) asm volatile ("cli; hlt");
    case ACPI_SIGNAL_BREAKPOINT:
        printk("[acpi] breakpoint: %s\n", (char *)Info);
        break;
    }
    return AE_OK;
}