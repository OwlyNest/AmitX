/*
	* exec/amx2.c - [Enter description]
	* Author:   amity
	* Date:     Mon Jul 20 10:42:42 2026
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
#include <exec/amx2.h>
#include <lib/string.h>
#include <screen/printk.h>
#include <internal/phonon_macros.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
static uint16_t get_u16(const uint8_t *buf, size_t offset);
static uint32_t get_u32(const uint8_t *buf, size_t offset);
/* --- Functions ---*/

static uint16_t get_u16(const uint8_t *buf, size_t offset) {
	return (uint16_t)buf[offset]
		| ((uint16_t)buf[offset + 1] << 8);
}

static uint32_t get_u32(const uint8_t *buf, size_t offset) {
    return (uint32_t)buf[offset]
         | ((uint32_t)buf[offset + 1] << 8)
         | ((uint32_t)buf[offset + 2] << 16)
         | ((uint32_t)buf[offset + 3] << 24);
}

/* ==========================================================================
 *                                                                          *
 * amx2_read_header()                                                       *
 *                                                                          *
 * Reads the AMX2 header from a raw buffer.                                 *
 * Does not validate — parsing only.                                        *
 * Returns 0 on success.                                                    *
 *                                                                          *
 * Offset  Size    Field                                                    *
 * -----------------------------------                                      *
 * 0x00    4       Magic                                                    *
 * 0x04    2       Version                                                  *
 * 0x06    2       Flags                                                    *
 * 0x08    2       Machine                                                  *
 * 0x0A    2       Subsystem                                                *
 * 0x0C    4       Entry                                                    *
 * 0x10    4       Preferred base                                           *
 * 0x14    4       Image size                                               *
 * 0x18    2       Section count                                            *
 * 0x1A    2       Reserved                                                 *
 * 0x1C    4       Section table offset                                     *
 * 0x20    4       Relocation table offset                                    *
 * 0x24    4       Relocation count                                         *
 * 0x28    4       Import table offset                                      *
 * 0x2C    4       Import count                                             *
 * 0x30    4       Export table offset                                      *
 * 0x34    4       Export count                                             *
 * 0x38    4       Export ordinal base                                      *
 * 0x3C    4       String table offset                                      *
 * 0x40    4       String table size                                        *
 * 0x44    32      Program name                                             *
 * 0x64    32      Author                                                   *
 * 0x7C    4       Checksum                                                 *
 *                                                                          *
 * Total: 128 bytes                                                         *
 *                                                                          *
 * ======================================================================== */
int amx2_read_header(const uint8_t *buf, amx2_header_t *header) {
	ASSERT(AMX2_HEADER_SIZE == (int)sizeof(amx2_header_t));

	if (!buf || !header) {
		return -1;
	}

	memcpy(header->magic, (buf + AMX2_OFF_MAGIC), 4);
	header->version             = get_u16(buf, AMX2_OFF_VERSION);
    header->flags               = get_u16(buf, AMX2_OFF_FLAGS);
    header->machine             = get_u16(buf, AMX2_OFF_MACHINE);
    header->subsystem           = get_u16(buf, AMX2_OFF_SUBSYSTEM);
    header->entry               = get_u32(buf, AMX2_OFF_ENTRY);
    header->preferred_base      = get_u32(buf, AMX2_OFF_PREFERRED_BASE);
    header->image_size          = get_u32(buf, AMX2_OFF_IMAGE_SIZE);
    header->section_count       = get_u16(buf, AMX2_OFF_SECTION_COUNT);
    header->reserved0           = get_u16(buf, AMX2_OFF_SECTION_COUNT + 2);
    header->section_offset      = get_u32(buf, AMX2_OFF_SECTION_OFFSET);
    header->reloc_offset        = get_u32(buf, AMX2_OFF_RELOC_OFFSET);
    header->reloc_count         = get_u32(buf, AMX2_OFF_RELOC_COUNT);
    header->import_offset       = get_u32(buf, AMX2_OFF_IMPORT_OFFSET);
    header->import_count        = get_u32(buf, AMX2_OFF_IMPORT_COUNT);
    header->export_offset       = get_u32(buf, AMX2_OFF_EXPORT_OFFSET);
    header->export_count        = get_u32(buf, AMX2_OFF_EXPORT_COUNT);
    header->export_ordinal_base = get_u32(buf, AMX2_OFF_EXPORT_ORD_BASE);
    header->string_offset       = get_u32(buf, AMX2_OFF_STRING_OFFSET);
    header->string_size         = get_u32(buf, AMX2_OFF_STRING_SIZE);
	memcpy(header->program_name, (buf + AMX2_OFF_PROGRAM_NAME), 32);
    memcpy(header->author, (buf + AMX2_OFF_AUTHOR), 32);
    header->checksum            = get_u32(buf, AMX2_OFF_CHECKSUM);

	return 0;
}

/* ==========================================================================
 *                                                                          *
 * amx2_verify_header()                                                     *
 *                                                                          *
 * Validates a parsed AMX2 header.                                          *
 * Returns AMX2_OK or an error code.                                        *
 *                                                                          *
 * ======================================================================== */
int amx2_verify_header(amx2_header_t *header) {
	if (!header) {
        return AMX2_BAD_HEADER;
    }

    if (memcmp(header->magic, "AMX\0", 4)) {
        return AMX2_BAD_MAGIC;
    }

    if (header->version != AMX2_VERSION) {
        return AMX2_BAD_VERSION;
    }

    /* Flags: must not have conflicting bits */
    if (header->flags & AMX2_FLAG_32BIT
        && header->flags & AMX2_FLAG_64BIT) {
        return AMX2_BAD_FLAGS;
    }

    /* Machine: must be known */
    if (header->machine != AMX2_MACHINE_I386
        && header->machine != AMX2_MACHINE_X86_64
        && header->machine != AMX2_MACHINE_ARM32
        && header->machine != AMX2_MACHINE_ARM64) {
        return AMX2_BAD_MACHINE;
    }

    /* Subsystem: must be known */
    if (header->subsystem > AMX2_SUBSYSTEM_GUI) {
        return AMX2_BAD_SUBSYSTEM;
    }

    /* Entry must be within image bounds */
    if (header->entry >= header->image_size) {
        return AMX2_BAD_ENTRY;
    }

    if (header->image_size == 0
        || header->image_size > AMX2_MAX_IMAGE_SIZE) {
        return AMX2_BAD_IMAGE_SIZE;
    }

    /* Section table must exist if sections claimed */
    if (header->section_count > AMX2_MAX_SECTIONS) {
        return AMX2_BAD_SECTION_COUNT;
    }

    if (header->section_count > 0
        && header->section_offset < AMX2_HEADER_SIZE) {
        return AMX2_BAD_SECTION_OFFSET;
    }

    /* Relocation table consistency */
    if (header->reloc_count == 0) {
        if (header->reloc_offset != 0) {
            return AMX2_BAD_RELOC_TBL;
        }
    } else {
        if (header->reloc_offset < AMX2_HEADER_SIZE) {
            return AMX2_BAD_RELOC_TBL;
        }
    }

    /* Import table consistency */
    if (header->import_count == 0) {
        if (header->import_offset != 0) {
            return AMX2_BAD_IMPORT_TBL;
        }
    } else {
        if (header->import_offset < AMX2_HEADER_SIZE) {
            return AMX2_BAD_IMPORT_TBL;
        }
    }

    /* Export table consistency */
    if (header->export_count == 0) {
        if (header->export_offset != 0) {
            return AMX2_BAD_EXPORT_TBL;
        }
    } else {
        if (header->export_offset < AMX2_HEADER_SIZE) {
            return AMX2_BAD_EXPORT_TBL;
        }
    }

    /* String table consistency */
    if (header->string_size == 0) {
        if (header->string_offset != 0) {
            return AMX2_BAD_STRING_TBL;
        }
    } else {
        if (header->string_offset < AMX2_HEADER_SIZE) {
            return AMX2_BAD_STRING_TBL;
        }
        if (header->string_size > AMX2_MAX_STRINGS) {
            return AMX2_BAD_STRING_TBL;
        }
    }

    /* Program name and author must be null-terminated */
    if (memchr(header->program_name, '\0', 32) == NULL) {
        return AMX2_BAD_HEADER;
    }

    if (memchr(header->author, '\0', 32) == NULL) {
        return AMX2_BAD_HEADER;
    }

    /* Checksum: verify later */

    return AMX2_OK;
}

/* ==========================================================================
 *                                                                          *
 * amx2_read_sections()                                                     *
 *                                                                          *
 * Reads the section table from file buffer.                                *
 * sections[] must be pre-allocated by caller (max_count entries).          *
 * Returns 0 on success, -1 on failure.                                     *
 *                                                                          *
 * ======================================================================== */
int amx2_read_sections(const uint8_t *buf, amx2_header_t *header, amx2_section_t *sections, uint32_t max_count) {
	if (!buf || !header || !sections) {
		return -1;
	}

	if (header->section_count > max_count) {
		return -1;
	}

	if (header->section_count == 0) {
		return 0;
	}

	uint32_t offset = header->section_offset;

	for (uint32_t i = 0; i < header->section_count; i++) {
		amx2_section_t *s = &sections[i];

		memcpy(s->name, (buf + offset), 8);
		offset += 8;
		s->flags       = get_u32(buf, offset);
		offset += 4;
		s->file_offset = get_u32(buf, offset);
		offset += 4;
        s->file_size   = get_u32(buf, offset);
        offset += 4;
        s->vaddr       = get_u32(buf, offset);
        offset += 4;
        s->mem_size    = get_u32(buf, offset);
        offset += 4;
        s->align       = get_u32(buf, offset);
        offset += 4;
	}

	return 0;
}

/* ==========================================================================
 *                                                                          *
 * amx2_verify_sections()                                                   *
 *                                                                          *
 * Validates section table against header and each other.                   *
 * Returns AMX2_OK or an error code.                                        *
 *                                                                          *
 * ======================================================================== */
int amx2_verify_sections(amx2_header_t *header, amx2_section_t *sections) {
	if (!header || !sections) {
		return AMX2_BAD_HEADER;
	}

	if (header->section_count == 0) {
        return AMX2_OK;
    }

	uint32_t prev_vaddr_end = 0;

	for (uint32_t i = 0; i < header->section_count; i++) {
		amx2_section_t *s = &sections[i];

		/* Name must be null-terminated within 8 bytes */
        if (memchr(s->name, '\0', 8) == NULL) {
            return AMX2_BAD_SECTION_NAME;
        }

        /* Flags: at least one of R/W/X */
        if ((s->flags & (AMX2_SECT_READ | AMX2_SECT_WRITE | AMX2_SECT_EXEC)) == 0) {
            return AMX2_BAD_SECTION_FLAGS;
        }

		/* BSS sections have no file data */
        if (s->flags & AMX2_SECT_BSS) {
            if (s->file_size != 0) {
                return AMX2_BAD_SECTION_FLAGS;
            }
        }

        /* Virtual address must be within image_size */
        if (s->vaddr >= header->image_size) {
            return AMX2_BAD_SECTION_VADDR;
        }

		if (s->vaddr + s->mem_size > header->image_size) {
            return AMX2_BAD_SECTION_VADDR;
        }

        /* mem_size must be >= file_size */
        if (s->mem_size < s->file_size) {
            return AMX2_BAD_SECTION_FLAGS;
        }

        /* File offset must be reasonable */
        if (s->file_size > 0
            && s->file_offset < AMX2_HEADER_SIZE) {
            return AMX2_BAD_SECTION_FILE_OFFSET;
        }

		/* Alignment must be power of 2 */
        if (s->align != 0) {
            if ((s->align & (s->align - 1)) != 0) {
                return AMX2_BAD_SECTION_FLAGS;
            }
        }

		/* Sections should not overlap in virtual space */
        if (i > 0) {
            if (s->vaddr < prev_vaddr_end) {
                return AMX2_BAD_SECTION_VADDR;
            }
        }
        prev_vaddr_end = s->vaddr + s->mem_size;
	}

	return AMX2_OK;
}

/* ==========================================================================
 *                                                                          *
 * amx2_print_header()                                                      *
 *                                                                          *
 * Pretty-prints an AMX2 header.                                            *
 *                                                                          *
 * ======================================================================== */
void amx2_print_header(amx2_header_t *header) {
    if (!header) {
        printk("[AMX2] no header\n");
        return;
    }

    printk("\n=== AMX2 Header ===\n");
    printk("Magic:               %s\n",     header->magic);
    printk("Version:             %u\n",      header->version);
    printk("Flags:               0x%04x\n",  header->flags);

    printk("Machine:             0x%04x\n", header->machine);
    printk("Subsystem:           %u\n",      header->subsystem);

    printk("Entry (RVA):         0x%08x\n",  header->entry);
    printk("Preferred base:      0x%08x\n",  header->preferred_base);
    printk("Image size:          %u bytes\n", header->image_size);

    printk("Sections:            %u\n",       header->section_count);
    printk("Section table at:    %u\n",       header->section_offset);

    printk("Relocations at:      %u\n",       header->reloc_offset);
    printk("Relocations:         %u\n",       header->reloc_count);

    printk("Imports at:          %u\n",       header->import_offset);
    printk("Imports:             %u\n",       header->import_count);

    printk("Exports at:          %u\n",       header->export_offset);
    printk("Exports:             %u\n",       header->export_count);
    printk("Export ord base:     %u\n",       header->export_ordinal_base);

    printk("String table at:     %u\n",       header->string_offset);
    printk("String table size:   %u\n",       header->string_size);

    printk("Program:             %s\n",       header->program_name);
    printk("Author:              %s\n",       header->author);

    printk("Checksum:            0x%08x\n",   header->checksum);
    printk("===================\n");
}

/* ==========================================================================
 *                                                                          *
 * amx2_print_section()                                                     *
 *                                                                          *
 * Pretty-prints a single section entry.                                    *
 *                                                                          *
 * ======================================================================== */
void amx2_print_section(amx2_section_t *section, uint32_t idx) {
    if (!section) {
        printk("[AMX2] no section\n");
        return;
    }

    printk("  [%u] %-8s  flags=0x%02x  "
           "file@%u+%u  vaddr=0x%08x  mem=%u  align=%u\n",
           idx,
           section->name,
           section->flags,
           section->file_offset,
           section->file_size,
           section->vaddr,
           section->mem_size,
           section->align);
}

