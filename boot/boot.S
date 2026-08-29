.set MB2_MAGIC,    0xE85250D6
.set MB2_ARCH,     0
.set MB2_LENGTH,   (mb2_header_end - mb2_header_start)
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)   

.section .multiboot, "a"
.align 8
mb2_header_start:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_LENGTH
    .long MB2_CHECKSUM  /* checksum */

/* Information request tag */
.align 8
mb2_tag_info:
    .short 1        /* type = information request */
    .short 1        /* flags: bit 0 = optional — don't hard-fail boot
                       if any requested tag can't be satisfied */
    .long 32        /* size = 8 + 4*num_requests */
    .long 4         /* basic memory info */
    .long 6         /* memory map */
    .long 8         /* framebuffer info */
    .long 9         /* ELF sections */
    .long 14        /* ACPI old RSDP */
    .long 15        /* ACPI new RSDP */
    
/* Skip for now, GRUB handles ELF kernels fine */
/* Optional: ask the bootloader to set a preferred mode.
 * GRUB2 will try to honor this on BIOS; on UEFI it usually
 * gives you the current GOP mode regardless. */
.align 8
mb2_tag_fb:
    .short 5        /* type = framebuffer */
    .short 0
    .long 20
    .long 1024      /* preferred width */
    .long 768       /* preferred height */
    .long 32        /* preferred bpp */

/* Null terminator */
.align 8
mb2_tag_end:
    .short 0
    .short 0
    .long 8
mb2_header_end:

.section .bss
.align 16
stack_bottom:
    #.skip 16384         # 16 KB stack
    .skip 65536         # 64 KB stack
stack_top:

.section .data
.align 4
.global multiboot_info_ptr
multiboot_info_ptr:
    .long 0

.section .text
.global _start
.type _start, @function

_start:
    cli

    /* Save multiboot info pointer (physical address) */
    movl %ebx, multiboot_info_ptr

    /* Verify Multiboot2 magic */
    cmpl $0x36d76289, %eax
    jne .no_multiboot2

    mov $stack_top, %esp

    push %ebx       /* mb_info */
    push %eax       /* magic */
    call kernel_early_init
    add $8, %esp

    call pmm_init
    test %eax, %eax
    jnz .pmm_fail

    call kernel_main
    jmp halt

.pmm_fail:
    movl $0xBAD01, %eax
    jmp halt

.no_multiboot2:
    /* Can't printk yet, but we can hang with a known EAX value */
    movl $0xDEADBEEF, %eax
halt:
    hlt
    jmp halt