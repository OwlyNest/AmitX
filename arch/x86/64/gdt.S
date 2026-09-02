# arch/x86/64/gdt.S - Global Descriptor Table (32-bit Long Mode)
# Author:   amity
# Date:     Mon Aug 31 12:19:51 2026
# Copyright © 2026 OwlyNest

# ==============================
# Alignment instructions:

# Instructions               begin at col 5
# First operand              begins at col 10
# Second operand             begins at col 20 (if possible)

# Header comments            begin at col 5
# In-line comments           begin at col 40
# Labels                     begin at col 1, end with ':'
# Section dividers           use 30 '=' characters
# (Sub)Section titles        use 3 '=' characters before and after

.intel_syntax noprefix

.global gdt_install
.global tss_set_rsp0

.global gdt_start
.global tss_struct
.global tss_end

/* --- Selectors --- */
.set SEL_KCODE, 0x08
.set SEL_KDATA, 0x10
.set SEL_UCODE, 0x18
.set SEL_UDATA, 0x20
.set SEL_TSS,   0x28
.set SEL_TSS_HI, 0x30

/* --- TSS (128 bytes for x86-64) --- */
.align 4
tss_struct:
    .long 0                  /* reserved / previous task link (unused) */
    .quad 0x90000            /* rsp0 - kernel stack (set by tss_set_rsp0) */
    .quad 0                  /* rsp1 */
    .quad 0                  /* rsp2 */
    .quad 0                  /* reserved */
    .quad 0                  /* ist1 */
    .quad 0                  /* ist2 */
    .quad 0                  /* ist3 */
    .quad 0                  /* ist4 */
    .quad 0                  /* ist5 */
    .quad 0                  /* ist6 */
    .quad 0                  /* ist7 */
    .quad 0                  /* reserved */
    .word 0                  /* reserved */
    .word 0x0068             /* i/o map base */
    .space 0x80 - (. - tss_struct)  /* pad to exactly 128 bytes */
tss_end:

/* Pre-compute TSS descriptor fields using .set */
.set tss_limit, (tss_end - tss_struct - 1)
.set tss_base_lo, (tss_struct & 0xFFFF)
.set tss_base_mid, ((tss_struct >> 16) & 0xFF)
.set tss_base_hi, ((tss_struct >> 24) & 0xFF)
.set tss_base_upper, ((tss_struct >> 32) & 0xFFFFFFFF)

/* --- GDT --- */
.align 4
gdt_start:
    /* 0: Null descriptor */
    .quad 0x0000000000000000

    /* 1: Kernel code (0x08) - long mode, ring 0 */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x9A
    .byte 0xAF               /* L=1, D=0 (long mode) */
    .byte 0x00

    /* 2: Kernel data (0x10) */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xCF               /* D=1, L=0 (data segment) */
    .byte 0x00

    /* 3: User code (0x18) - long mode, ring 3 */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0xFA
    .byte 0xAF               /* L=1, D=0 */
    .byte 0x00

    /* 4: User data (0x20) */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0xF2
    .byte 0xCF
    .byte 0x00

    /* 5: TSS low (0x28) */
    .word tss_limit
    .word tss_base_lo
    .byte tss_base_mid
    .byte 0x89
    .byte 0x00
    .byte tss_base_hi

    /* 6: TSS high (0x30) */
    .long tss_base_upper
    .long 0x00000000
gdt_end:

gdt_ptr:
    .word gdt_end - gdt_start - 1
    .quad gdt_start

/* --- Functions --- */

gdt_install:
    lgdt [gdt_ptr]
    mov ax, SEL_TSS
    ltr ax
    mov ax, SEL_KDATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp SEL_KCODE:flush_cs

flush_cs:
    ret

/* void tss_set_rsp0(uint64_t rsp) */
tss_set_rsp0:
    mov [tss_struct + 4], rdi    /* rdi = first arg in SysV AMD64 ABI */
    ret