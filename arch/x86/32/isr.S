# arch/x86/32/isr.S - Interrupt Service Routines (32-bit)
# Author:   amity
# Date:     Mon Aug 31 14:39:00 2026
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

.global load_idt
.global panic_stack_top

/* --- ISR exports --- */
.global isr0
.global isr1
.global isr2
.global isr3
.global isr4
.global isr5
.global isr6
.global isr7
.global isr8
.global isr9
.global isr10
.global isr11
.global isr12
.global isr13
.global isr14
.global isr15
.global isr16
.global isr17
.global isr18
.global isr19
.global isr20
.global isr21
.global isr22
.global isr23
.global isr24
.global isr25
.global isr26
.global isr27
.global isr28
.global isr29
.global isr30
.global isr31

.global isr32
.global isr33
.global isr34
.global isr35
.global isr36
.global isr37
.global isr38
.global isr39
.global isr40
.global isr41
.global isr42
.global isr43
.global isr44
.global isr45
.global isr46
.global isr47
.global isr128

.extern isr_handler

/* ==========================================================================
 * Macros
 * ======================================================================= */

/* ISR with no error code */
.macro ISR_NO_ERRCODE num
isr\num:
    cli
    pushad

    push 0              /* Fake error code */
    push \num           /* Interrupt number */

    push esp
    call isr_handler
    add esp, 12         /* 4 (arg) + 8 (err + num) */

    popad
    iret
.endm

/* ISR with error code pushed by CPU */
.macro ISR_ERRCODE num
isr\num:
    cli
    pushad

    /* Error code is at [esp + 32] after pushad (8 regs * 4 bytes) */
    push dword ptr [esp + 32]
    push \num

    push esp
    call isr_handler
    add esp, 12

    popad
    add esp, 4          /* Discard original CPU error code */
    iret
.endm

/* IRQ stub (no error code, no CLI needed) */
.macro IRQ_STUB num
isr\num:
    pushad

    push 0
    push \num

    push esp
    call isr_handler
    add esp, 12

    popad
    iret
.endm

/* ==========================================================================
 * Exception ISRs 0-31
 * ======================================================================= */
ISR_NO_ERRCODE 0
ISR_NO_ERRCODE 1
ISR_NO_ERRCODE 2
ISR_NO_ERRCODE 3
ISR_NO_ERRCODE 4
ISR_NO_ERRCODE 5
ISR_NO_ERRCODE 6
ISR_NO_ERRCODE 7
ISR_NO_ERRCODE 9
ISR_ERRCODE    10
ISR_ERRCODE    11
ISR_ERRCODE    12
ISR_ERRCODE    13
ISR_ERRCODE    14
ISR_NO_ERRCODE 15
ISR_NO_ERRCODE 16
ISR_ERRCODE    17
ISR_NO_ERRCODE 18
ISR_NO_ERRCODE 19
ISR_NO_ERRCODE 20
ISR_NO_ERRCODE 21
ISR_NO_ERRCODE 22
ISR_NO_ERRCODE 23
ISR_NO_ERRCODE 24
ISR_NO_ERRCODE 25
ISR_NO_ERRCODE 26
ISR_NO_ERRCODE 27
ISR_NO_ERRCODE 28
ISR_NO_ERRCODE 29
ISR_NO_ERRCODE 30
ISR_NO_ERRCODE 31

/* ==========================================================================
 * Double fault (vector 8) - switch to panic stack
 * ======================================================================= */
isr8:
    mov eax, esp            /* Save original esp */
    mov esp, offset panic_stack_top

    push 0                  /* eax */
    push 0                  /* ecx */
    push 0                  /* edx */
    push 0                  /* ebx */
    push eax                /* original esp */
    push 0                  /* ebp */
    push 0                  /* esi */
    push 0                  /* edi */

    push 0                  /* Fake error code */
    push 8                  /* Interrupt number */

    push esp
    call isr_handler
    add esp, 12

    hlt                     /* Don't return from double fault */
    jmp .

/* ==========================================================================
 * IRQ ISRs 32-47
 * ======================================================================= */
IRQ_STUB 32
IRQ_STUB 33
IRQ_STUB 34
IRQ_STUB 35
IRQ_STUB 36
IRQ_STUB 37
IRQ_STUB 38
IRQ_STUB 39
IRQ_STUB 40
IRQ_STUB 41
IRQ_STUB 42
IRQ_STUB 43
IRQ_STUB 44
IRQ_STUB 45
IRQ_STUB 46
IRQ_STUB 47

/* ==========================================================================
 * Syscall (int 0x80)
 * ======================================================================= */
isr128:
    cld
    pushad

    push 0
    push 128

    push esp
    call isr_handler
    add esp, 12

    popad
    iret

/* ==========================================================================
 * load_idt(uint32_t idt_ptr)
 * ======================================================================= */
load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    ret

/* ==========================================================================
 * Panic stack (double fault)
 * ======================================================================= */
.section .bss
.align 16
panic_stack:
    .space 4096
panic_stack_top: