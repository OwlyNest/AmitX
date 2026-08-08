.intel_syntax noprefix
.global gdt_install
.global tss_set_esp0

.global gdt_start
.global tss_struct
.global tss_end

/* --- Selectors --- */
.set SEL_KCODE, 0x08
.set SEL_KDATA, 0x10
.set SEL_UCODE, 0x18
.set SEL_UDATA, 0x20
.set SEL_TSS,   0x28

/* --- TSS --- */
.align 4
tss_struct:
    .word 0                  /* previous task link */
    .word 0                  /* reserved */
    .long 0x90000            /* esp0 - kernel stack (set by tss_set_esp0) */
    .word SEL_KDATA          /* ss0 */
    .word 0                  /* reserved */
    .long 0                  /* esp1 */
    .word 0                  /* ss1 */
    .word 0                  /* reserved */
    .long 0                  /* esp2 */
    .word 0                  /* ss2 */
    .word 0                  /* reserved */
    .long 0                  /* cr3 */
    .long 0                  /* eip */
    .long 0                  /* eflags */
    .long 0                  /* eax */
    .long 0                  /* ecx */
    .long 0                  /* edx */
    .long 0                  /* ebx */
    .long 0                  /* esp */
    .long 0                  /* ebp */
    .long 0                  /* esi */
    .long 0                  /* edi */
    .word SEL_KDATA          /* es */
    .word 0                  /* reserved */
    .word SEL_KCODE          /* cs */
    .word 0                  /* reserved */
    .word SEL_KDATA          /* ss */
    .word 0                  /* reserved */
    .word SEL_KDATA          /* ds */
    .word 0                  /* reserved */
    .word SEL_KDATA          /* fs */
    .word 0                  /* reserved */
    .word SEL_KDATA          /* gs */
    .word 0                  /* reserved */
    .word 0                  /* ldt selector */
    .word 0                  /* reserved */
    .word 0                  /* debug trap */
    .word 0x0068             /* i/o map base (104 bytes = end of TSS) */
tss_end:

/* --- GDT --- */
.align 4
gdt_start:
    /* 0: Null descriptor */
    .quad 0x0000000000000000

    /* 1: Kernel code (0x08) - base=0, limit=4GB, ring 0, execute/read */
    .word 0xFFFF             /* limit low */
    .word 0x0000             /* base low */
    .byte 0x00               /* base middle */
    .byte 0x9A               /* access: present, ring 0, code, execute/read */
    .byte 0xCF               /* granularity: 4KB, 32-bit, limit high */
    .byte 0x00               /* base high */

    /* 2: Kernel data (0x10) - base=0, limit=4GB, ring 0, read/write */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x92               /* access: present, ring 0, data, read/write */
    .byte 0xCF
    .byte 0x00

    /* 3: User code (0x18) - base=0, limit=4GB, ring 3, execute/read */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0xFA               /* access: present, ring 3, code, execute/read */
    .byte 0xCF
    .byte 0x00

    /* 4: User data (0x20) - base=0, limit=4GB, ring 3, read/write */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0xF2               /* access: present, ring 3, data, read/write */
    .byte 0xCF
    .byte 0x00

    /* 5: TSS (0x28) - (fill at runtime) */
    .quad 0
gdt_end:

gdt_ptr:
    .word gdt_end - gdt_start - 1
    .long gdt_start

tss_ptr:
    .word tss_end - tss_struct - 1
    .long tss_struct

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

/* void tss_set_esp0(uint32_t esp) */
tss_set_esp0:
    mov eax, [esp + 4]       /* get esp argument */
    mov [tss_struct + 4], eax /* tss_struct.esp0 is at offset 4 */
    ret