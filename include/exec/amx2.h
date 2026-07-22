/*
	* include/exec/amx2.h - [Enter description]
	* Author:   amity
	* Date:     Mon Jul 20 10:42:17 2026
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
#ifndef __EXEC_AMX2_H__
#define __EXEC_AMX2_H__

#include <stdarg.h>
#define AMX2_VERSION              2

#define AMX2_HEADER_SIZE          128
#define AMX2_SECTION_SIZE         32
#define AMX2_RELOC_SIZE           16
#define AMX2_IMPORT_SIZE          12
#define AMX2_EXPORT_SIZE          16

#define AMX2_MAX_IMAGE_SIZE       (16 * 1024 * 1024) // 16 MB
#define AMX2_MAX_SECTIONS         32  // Doesn't sound like a lot? The entire OS currently has 18
#define AMX2_MAX_STRINGS          (64 * 1024) // 64 kB = full segment

/* --- Header field offsets ---*/
#define AMX2_OFF_MAGIC            0x00
#define AMX2_OFF_VERSION          0x04
#define AMX2_OFF_FLAGS            0x06
#define AMX2_OFF_MACHINE          0x08
#define AMX2_OFF_SUBSYSTEM        0x0A
#define AMX2_OFF_ENTRY            0x0C
#define AMX2_OFF_PREFERRED_BASE   0x10
#define AMX2_OFF_IMAGE_SIZE       0x14
#define AMX2_OFF_SECTION_COUNT    0x18
#define AMX2_OFF_SECTION_OFFSET   0x1C
#define AMX2_OFF_RELOC_OFFSET     0x20
#define AMX2_OFF_RELOC_COUNT      0x24
#define AMX2_OFF_IMPORT_OFFSET    0x28
#define AMX2_OFF_IMPORT_COUNT     0x2C
#define AMX2_OFF_EXPORT_OFFSET    0x30
#define AMX2_OFF_EXPORT_COUNT     0x34
#define AMX2_OFF_EXPORT_ORD_BASE  0x38
#define AMX2_OFF_STRING_OFFSET    0x3C
#define AMX2_OFF_STRING_SIZE      0x40
#define AMX2_OFF_PROGRAM_NAME     0x44
#define AMX2_OFF_AUTHOR           0x64
#define AMX2_OFF_CHECKSUM         0x7C

/* --- Header flags ---*/
#define AMX2_FLAG_RELOC_STRIPPED  0x0001
#define AMX2_FLAG_DEBUG_INFO      0x0002
#define AMX2_FLAG_32BIT           0x0004
#define AMX2_FLAG_64BIT           0x0008
#define AMX2_FLAG_ENTRY_IS_VADDR  0x0010

/* --- Machine types ---*/
#define AMX2_MACHINE_I386         0x014C
#define AMX2_MACHINE_X86_64       0x8664
#define AMX2_MACHINE_ARM32        0x01C0
#define AMX2_MACHINE_ARM64        0xAA64

/* --- Subsystems ---*/
#define AMX2_SUBSYSTEM_NATIVE     0
#define AMX2_SUBSYSTEM_CONSOLE    1
#define AMX2_SUBSYSTEM_GUI        2

/* --- Section flags ---*/
#define AMX2_SECT_READ            0x01
#define AMX2_SECT_WRITE           0x02
#define AMX2_SECT_EXEC            0x04
#define AMX2_SECT_BSS             0x08
#define AMX2_SECT_SHARED          0x10
#define AMX2_SECT_DISCARDABLE     0x20
#define AMX2_SECT_NOCACHE         0x40

/* --- Relocation types ---*/
#define AMX2_RELOC_ABS32          0
#define AMX2_RELOC_REL32          1
#define AMX2_RELOC_IMPORT         2
#define AMX2_RELOC_HIGH16         3
#define AMX2_RELOC_LOW16          4

/* --- Import flags ---*/
#define AMX2_IMPORT_BY_ORDINAL    0x0001

/* --- Export flags ---*/
#define AMX2_EXPORT_BY_ORDINAL    0x0001

/* --- Includes ---*/
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* ==========================================================================
 * AMX2 Header (128 bytes)
 * ========================================================================== */
 typedef struct {
    char     magic[4];          /* "AMX\0"                    */
    uint16_t version;           /* AMX2_VERSION               */
    uint16_t flags;             /* AMX2_FLAG_*                */

    uint16_t machine;           /* AMX2_MACHINE_*             */
    uint16_t subsystem;         /* AMX2_SUBSYSTEM_*           */

    uint32_t entry;             /* RVA of _start              */

    uint32_t preferred_base;    /* Desired load address       */
    uint32_t image_size;        /* Total virtual memory size  */

    uint16_t section_count;
    uint16_t reserved0;
    uint32_t section_offset;    /* File offset to section tbl */

    uint32_t reloc_offset;      /* File offset to reloc tbl   */
    uint32_t reloc_count;

    uint32_t import_offset;     /* File offset to import tbl  */
    uint32_t import_count;

    uint32_t export_offset;     /* File offset to export tbl  */
    uint32_t export_count;
    uint32_t export_ordinal_base;

    uint32_t string_offset;     /* File offset to string tbl  */
    uint32_t string_size;

    char     program_name[32];
    char     author[32];

    uint32_t checksum;          /* CRC32 of bytes 0x00-0x7B   */
} amx2_header_t;

/* ==========================================================================
 * Section Table Entry (32 bytes)
 * ========================================================================== */
typedef struct {
	char     name[8];           /* e.g., ".text\0\0\0"        */
    uint32_t flags;             /* AMX2_SECT_*                */
    uint32_t file_offset;       /* In file                    */
    uint32_t file_size;         /* Bytes in file              */
    uint32_t vaddr;             /* RVA from image base        */
    uint32_t mem_size;          /* Total in memory            */
    uint32_t align;             /* Power of 2                 */
} amx2_section_t;

/* ==========================================================================
 * Relocation Entry (16 bytes)
 * ========================================================================== */
 typedef struct {
    uint32_t offset;            /* Offset within section      */
    uint16_t section_idx;       /* Target section             */
    uint16_t type;              /* AMX2_RELOC_*               */
    uint32_t addend;            /* Signed addend              */
    uint32_t info;              /* Import ordinal / sect idx  */
} amx2_reloc_t;

/* ==========================================================================
 * Import Entry (12 bytes)
 * ========================================================================== */
typedef struct {
    uint32_t module_name;       /* Offset into string table   */
    uint32_t symbol_name;       /* Offset into string table   */
    uint16_t ordinal;           /* For ordinal imports        */
    uint16_t flags;             /* AMX2_IMPORT_*              */
} amx2_import_t;

/* ==========================================================================
 * Export Entry (16 bytes)
 * ========================================================================== */
typedef struct {
    uint32_t name_offset;       /* Offset into string table   */
    uint16_t ordinal;           /* Export ordinal             */
    uint16_t flags;             /* AMX2_EXPORT_*              */
    uint16_t section_idx;       /* Which section              */
    uint16_t reserved;
    uint32_t offset;            /* Offset within section      */
} amx2_export_t;

/* ==========================================================================
 * Status codes
 * ========================================================================== */
typedef enum {
    AMX2_OK = 0,

    AMX2_BAD_HEADER,
    AMX2_BAD_MAGIC,
    AMX2_BAD_VERSION,
    AMX2_BAD_FLAGS,
    AMX2_BAD_MACHINE,
    AMX2_BAD_SUBSYSTEM,
    AMX2_BAD_ENTRY,
    AMX2_BAD_IMAGE_SIZE,
    AMX2_BAD_SECTION_COUNT,
    AMX2_BAD_SECTION_OFFSET,
    AMX2_BAD_RELOC_TBL,
    AMX2_BAD_RELOC_NUM,
    AMX2_BAD_IMPORT_TBL,
    AMX2_BAD_EXPORT_TBL,
    AMX2_BAD_STRING_TBL,
    AMX2_BAD_CHECKSUM,
    AMX2_BAD_SECTION_NAME,
    AMX2_BAD_SECTION_FLAGS,
    AMX2_BAD_SECTION_VADDR,
    AMX2_BAD_SECTION_FILE_OFFSET,
    AMX2_BAD_RELOC_SECTION,
    AMX2_BAD_RELOC_OFFSET,
    AMX2_BAD_RELOC_TYPE
} amx2_status_t;
/* --- Globals ---*/

/* --- Prototypes ---*/
int  amx2_read_header(const uint8_t *buf, amx2_header_t *header);
int  amx2_verify_header(amx2_header_t *header);
int  amx2_read_sections(const uint8_t *buf, amx2_header_t *header, amx2_section_t *sections, uint32_t max_count);
int  amx2_verify_sections(amx2_header_t *header, amx2_section_t *sections);
void amx2_print_header(amx2_header_t *header);
void amx2_print_section(amx2_section_t *section, uint32_t idx);
#endif