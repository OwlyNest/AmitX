
.set MAGIC, 0x1BADB002
.set FLAGS, (0x00000001 | 0x00000002)
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
    .align 4
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

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

    movl %ebx, multiboot_info_ptr

    mov $stack_top, %esp   # Set stack pointer

    push %ebx
    call kernel_early_init
    add $4, %esp
    
    call kernel_main       # Enter kernel


halt:
    hlt
    jmp halt
