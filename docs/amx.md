# AMX

AMX (AmitX eXecutable) is the native executable format used by the AmitX operating system.

An `.AMX` file contains everything required for the AmitX loader to create and start a process: executable machine code, initialized data, and relocation information. The format is intentionally simple and is designed specifically for AmitX rather than for compatibility with existing executable formats such as ELF or PE.

Unless otherwise specified, all multi-byte integers are stored in little-endian format.

The `.AMX` executable format consists of three parts:

```none
┏━━━━━━━━━━━━━━━━━━━━━━━┓
┃ Header                ┃
┣━━━━━━━━━━━━━━━━━━━━━━━┫
┃ Image                 ┃
┣━━━━━━━━━━━━━━━━━━━━━━━┫
┃ Relocation table      ┃
┗━━━━━━━━━━━━━━━━━━━━━━━┛
```

## Design goals

AMX is designed to be:

- Simple to generate.
- Simple to load.
- Fast to execute.
- Specific to the AmitX operating system.

The format intentionally omits features that are unnecessary for AmitX. Additional capabilities may be introduced through new format versions while maintaining backwards compatibility where possible.

## Versioning

The AMX format is versioned.

Loaders must reject executables with a version number they do not support.
Future versions may extend the format while preserving compatibility where practical.

## Header

Version 1 of the AMX format defines a fixed-size header of 104 bytes. It consists of 13 fields. With the following exact layout.

```none
Offset  Size    Field                      Type
----------------------------------------------------
0x00    4       Magic                      char[4]
0x04    2       Version                    uint16_t
0x06    2       Flags                      uint16_t

0x08    4       Image offset               uint32_t
0x0C    4       Image size                 uint32_t
0x10    4       Entry                      uint32_t

0x14    4       BSS size                   uint32_t
0x18    4       Stack size                 uint32_t

0x1C    4       Relocation table offset    uint32_t
0x20    4       Number of relocations      uint32_t

0x24    32      Program name               char[32]
0x44    32      Author                     char[32]

0x64    4       Checksum                   uint32_t
```

The magic number at the start of the file is "AMX\0" (0x41 0x4D 0x58 0x00). The current version is 1 (0x01 0x00). The current version of the AMX format does not support flags and must be 0 (0x00 0x00).

Offsets stored in the header are relative to the beginning of the file.

Offsets stored inside the image, such as relocation targets, are relative to the beginning of the image

The image immediately follows the header. Therefore, in version 1 of the format, the image offset is always 104 bytes (0x68 0x00 0x00 0x00).

The image size specifies the total number of bytes contained in the image.

The entry is the ofset from the start of the image where the entry point of the code is. version 1 of the format doesn't support an entry symbol and always enters the code from the beginning, the entry must therefore be 0 (0x00 0x00 0x00 0x00).

BSS size specifies the number of zero-initialized bytes that the loader allocates immediately after the image.

Stack size is the number of bytes that the loade allocates as the programs stack.

Relocation table offset is the offset from the start of the file at which the relocation table is located, this is the header size + the image size.

The number of relocations specifies the number of relocation entries that follow the image.

The checksum field is reserved for future use. Version 1 assemblers write this field as zero, and loaders currently ignore it.

Version 1 imposes no alignment requirements beyond the natural alignment of the header.

### C representation

```c
typedef struct {
    char     magic[4];          /* "AMX\0"                    */
    uint16_t version;           /* AMX_VERSION                */
    uint16_t flags;

    uint32_t image_offset;
    uint32_t image_size;        /* code + data               */

    uint32_t entry;             /* offset into image         */

    uint32_t bss_size;
    uint32_t stack_size;

    uint32_t reloc_offset;      /* from file start           */
    uint32_t reloc_count;

    char     program_name[32];
    char     author[32];

    uint32_t checksum;
} amx_header_t;
```

## Image

The image is a contiguous block of bytes containing executable code and initialized data. Version 1 does not distinguish between code and data sections; both are stored in a single flat image.

## Relocation table

The relocation table immediately follows the image and contains all relocations required by the loader before execution begins.

Each relocation entry has the following layout:

```none
Size    Field       Type
-----------------------------
4       Offset      uint32_t
2       Type        uint16_t
2       Reserved    uint16_t
```

### Offset

The offset, relative to the beginning of the image, of the value that must be relocated.

### Type

The relocation type determines how the loader adjusts the value stored at the relocation offset.

Version 1 defines the following relocation types:

```none
Value   Name
----------------------
0       AMX_RELOC_ABS32
```

`AMX_RELOC_ABS32` instructs the loader to relocate a 32-bit absolute address by adding the base address of the loaded image.

### Reserved

This field is reserved for future use.

Version 1 assemblers must write this field as zero. Version 1 loaders must ignore its value.
