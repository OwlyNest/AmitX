
.intel_syntax noprefix
.global gdt_install

gdt_start:
    .quad 0x0000000000000000  # Null descriptor
    .quad 0x00cf9a000000ffff  # Code segment descriptor
    .quad 0x00cf92000000ffff  # Data segment descriptor
gdt_end:

gdt_ptr:
    .word gdt_end - gdt_start - 1
    .long gdt_start

gdt_install:
    lgdt [gdt_ptr]
    mov ax, 0x10       # Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:flush_cs

flush_cs:
    ret
