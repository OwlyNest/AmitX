ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R + X */
    data PT_LOAD FLAGS(6);   /* R + W */
}

SECTIONS
{
    . = 0x100000;

    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text*)
    } :text

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    } :text

    .data : ALIGN(4K)
    {
        *(.data*)
    } :data

    .bss : ALIGN(4K)
    {
        *(.bss*)
        *(COMMON)
    } :data

    _end = .;
}