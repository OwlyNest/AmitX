
.set MAGIC, 0x1BADB002
.set FLAGS, 0
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

.section .bss
.align 16
stack_bottom:
    .skip 16384         # 16 KB stack
stack_top:

.section .text
.global _start
.type _start, @function

_start:
    cli

    mov $stack_top, %esp   # Set stack pointer

    call kernel_main       # Enter kernel


halt:
    hlt
    jmp halt
