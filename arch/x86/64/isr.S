# arch/x86/64/isr.S - Interrupt Service Routines (64-bit)
# Author:   amity
# Date:     Mon Aug 31 14:39:04 2026
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

/* 
 * In 64-bit long mode, the CPU auto-pushes on interrupt:
 *   ss, rsp, rflags, cs, rip
 * If error code: also pushes error_code
 * 
 * We need to manually push all GPRs to build interrupt_frame_t.
 * 
 * Frame layout on stack after SAVE_ALL:
 *   ss          (CPU)
 *   rsp         (CPU)
 *   rflags      (CPU)
 *   cs          (CPU)
 *   rip         (CPU)
 *   [err_code]  (CPU, if applicable)
 *   r15-r8      (manual)
 *   rdi, rsi, rbp, rbx, rdx, rcx, rax  (manual)
 *   gs, fs      (manual segment selectors)
 *   err_no      (manual - we push this)
 *   err_code    (manual - we push this)
 */

.macro SAVE_ALL
    /* Push all GPRs */
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax

    /* Push segment selectors */
    mov ax, gs
    push rax
    mov ax, fs
    push rax
.endm

.macro RESTORE_ALL
    /* Pop segment selectors (discard, segments restored by iretq) */
    add rsp, 16

    /* Pop all GPRs */
    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
.endm

/* ISR with no error code */
.macro ISR_NO_ERRCODE num
isr\num:
    cli
    cld
    SAVE_ALL
    push 0
    push \num
    mov rdi, rsp
    call isr_handler
    add rsp, 16
    RESTORE_ALL
    iretq
.endm

/* ISR with error code pushed by CPU */
.macro ISR_ERRCODE num
isr\num:
    cli
    cld
    SAVE_ALL
    /* CPU already pushed error code, it's now under our saves */
    /* Fetch it: after SAVE_ALL (15*8 + 2*8 = 136 bytes), then err_code */
    mov rax, [rsp + 136]
    push rax
    push \num
    mov rdi, rsp
    call isr_handler
    add rsp, 16
    RESTORE_ALL
    add rsp, 8          /* Remove CPU's error code */
    iretq
.endm

/* IRQ stub */
.macro IRQ_STUB num
isr\num:
    cli
    cld

    test qword ptr [rsp + 16], 3
    jz 1f
    swapgs
1:
    SAVE_ALL

    push 0
    push \num

    mov rdi, rsp
    call isr_handler

    add rsp, 16

    RESTORE_ALL

    test qword ptr [rsp + 16], 3
    jz 2f
    swapgs
2:
    iretq
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
 * Double fault (vector 8) - uses IST1, no manual stack switch needed
 * ======================================================================= */
isr8:
    /* IST1 already switched us to a safe stack */
    cli
    cld

    /* swapgs not needed - double fault only from kernel, or if from user,
     * the CPU already handled it via IST. But check anyway.
     */
    test qword ptr [rsp + 24], 3
    jz 1f
    swapgs
1:
    SAVE_ALL

    /* CPU pushed error code for #DF */
    mov rax, [rsp + 15*8 + 2*8]
    push rax
    push 8

    mov rdi, rsp
    call isr_handler

    /* Don't return from double fault */
    hlt
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
 * Syscall (int 0x80 legacy, or syscall instruction)
 * ======================================================================= */
isr128:
    /* Legacy int 0x80 entry */
    cli
    cld

    /* For int 0x80, we're always from user mode, so swapgs */
    swapgs

    SAVE_ALL

    push 0
    push 128

    mov rdi, rsp
    call isr_handler

    add rsp, 16

    RESTORE_ALL

    swapgs
    iretq

/* ==========================================================================
 * load_idt(uint64_t idt_ptr)
 * ======================================================================= */
load_idt:
    /* First argument in rdi (SysV AMD64 ABI) */
    lidt [rdi]
    ret