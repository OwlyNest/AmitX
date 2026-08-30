ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x00100000;

    .multiboot ALIGN(8) :
    {
        KEEP(*(.multiboot))
    } :text

    .boot.text ALIGN(4K) :
    {
        *(.boot.text*)
    } :text

    .boot.data ALIGN(4K) :
    {
        *(.boot.data*)
    } :data

    .boot.bss ALIGN(4K) (NOLOAD) :
    {
        *(.boot.bss*)
    } :data

    __kernel_phys_start = 0x00100000;
    . = ALIGN(__kernel_phys_start + SIZEOF(.multiboot) +
              SIZEOF(.boot.text) + SIZEOF(.boot.data) +
              SIZEOF(.boot.bss), 4K) + 0xC0000000;

    .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000)
    {
        *(.text*)
    } :text

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000)
    {
        *(.rodata*)
    } :text

    .data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000)
    {
        *(.data*)
    } :data

    .bss ALIGN(4K) (NOLOAD) : AT(ADDR(.bss) - 0xC0000000)
    {
        *(.bss*)
        *(COMMON)
    } :data

    _kernel_phys_end = LOADADDR(.bss) + SIZEOF(.bss);
    _end = .;
}