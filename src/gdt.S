
.intel_syntax noprefix
.global gdt_install

# AmitGFX has a bigger and better one, including a TSS
gdt_start:
    .quad 0x0000000000000000 
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
gdt_end:

gdt_ptr:
    .word gdt_end - gdt_start - 1
    .long gdt_start

gdt_install:
    lgdt [gdt_ptr]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:flush_cs

flush_cs:
    ret
