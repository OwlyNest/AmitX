ENTRY(_start)

PHDRS
{
    boot_text PT_LOAD FLAGS(5);
    boot_data PT_LOAD FLAGS(6);
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
    bss PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x00100000;

    .multiboot ALIGN(8) :
    {
        KEEP(*(.multiboot))
    } :boot_text

    .boot.text ALIGN(4K) :
    {
        *(.boot.text*)
    } :boot_text

    .boot.data ALIGN(4K) :
    {
        *(.boot.data*)
    } :boot_data

    .boot.bss 0x00110000 (NOLOAD) :
    {
        *(.boot.bss*)
    } :boot_data

    __kernel_phys_start = 0x00200000;

    . = __kernel_phys_start + 0xC0000000;

    .text ALIGN(4K) : AT(__kernel_phys_start)
    {
        *(.text*)
    } :text

    .rodata ALIGN(4K) : AT(ALIGN(LOADADDR(.text) + SIZEOF(.text), 4K))
    {
        *(.rodata*)
        *(.eh_frame*)
    } :rodata

    .data ALIGN(4K) : AT(ALIGN(LOADADDR(.rodata) + SIZEOF(.rodata), 4K))
    {
        *(.data*)
    } :data

    .bss ALIGN(4K) (NOLOAD) : AT(ALIGN(LOADADDR(.data) + SIZEOF(.data), 4K))
    {
        *(.bss*)
        *(COMMON)
    } :bss

    _kernel_phys_end = LOADADDR(.bss) + SIZEOF(.bss);
    _end = .;
}