ENTRY(_start)

SECTIONS {
    /* Higher-half virtual address */
    . = 0xC0100000;

    .text : AT(0x100000) {
        *(.multiboot)
        *(.text*)
    }

    .rodata : AT(LOADADDR(.text) + SIZEOF(.text)) {
        *(.rodata*)
    }

    .data : AT(LOADADDR(.rodata) + SIZEOF(.rodata)) {
        *(.data*)
    }

    .bss : AT(LOADADDR(.data) + SIZEOF(.data)) ALIGN(4K) {
        *(.bss*)
        *(COMMON)
    }

    _end = .;
}