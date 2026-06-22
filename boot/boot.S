.set MB2_MAGIC, 0xE85250D6
.set MB2_ARCH,  0          /* i386 protected mode */
.set MB2_LENGTH, (mb2_header_end - mb2_header_start)

.section .multiboot
.align 8
mb2_header_start:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_LENGTH
    .long -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)  /* checksum */

    /* Information request tag */
    .align 8
    mb2_tag_info:
        .short 1        /* type = information request */
        .short 0        /* flags */
        .long 24        /* size = 8 + 4*num_requests */
        .long 4         /* basic memory info */
        .long 6         /* memory map */
        .long 9         /* ELF sections */

    /* Address tag — required for non-ELF or if you want explicit load addresses */
    /* Skip for now, GRUB handles ELF kernels fine */

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
    .skip 16384         # 16 KB stack
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

    /* Save multiboot info pointer */
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