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
    push fs
    push gs
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
.endm

.macro RESTORE_ALL
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 16
.endm

/* Build interrupt_frame_t above the CPU's original return frame. */
.macro BUILD_FRAME_NOERR num
    push 0
    push 0
    push qword ptr [rsp + 32]
    push qword ptr [rsp + 32]
    push qword ptr [rsp + 32]
    SAVE_ALL
    push 0
    push \num
.endm

/* Build interrupt_frame_t when the CPU supplied an error code. */
.macro BUILD_FRAME_ERR num
    push 0
    push 0
    push qword ptr [rsp + 40]
    push qword ptr [rsp + 40]
    push qword ptr [rsp + 40]
    SAVE_ALL
    push qword ptr [rsp + 176]
    push \num
.endm

.macro DISCARD_FRAME
    add rsp, 16
    RESTORE_ALL
    add rsp, 40
.endm

.macro CALL_HANDLER
    mov r11, rsp
    and rsp, -16
    sub rsp, 16
    mov qword ptr [rsp], r11
    mov rdi, r11
    call isr_handler
    mov rsp, qword ptr [rsp]
.endm

/* ISR with no error code */
.macro ISR_NO_ERRCODE num
isr\num:
    cli
    cld
    test qword ptr [rsp + 8], 3
    jz 1f
    swapgs
1:
    BUILD_FRAME_NOERR \num
    CALL_HANDLER
    DISCARD_FRAME
    test qword ptr [rsp + 8], 3
    jz 2f
    swapgs
2:
    iretq
.endm

/* ISR with error code pushed by CPU */
.macro ISR_ERRCODE num
isr\num:
    cli
    cld
    test qword ptr [rsp + 16], 3
    jz 1f
    swapgs
1:
    BUILD_FRAME_ERR \num
    CALL_HANDLER
    DISCARD_FRAME
    test qword ptr [rsp + 16], 3
    jz 2f
    swapgs
2:
    iretq
.endm

/* IRQ stub */
.macro IRQ_STUB num
isr\num:
    cli
    cld

    test qword ptr [rsp + 8], 3
    jz 1f
    swapgs
1:
    BUILD_FRAME_NOERR \num
    CALL_HANDLER

    DISCARD_FRAME

    test qword ptr [rsp + 8], 3
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

/* Double fault uses IST1 and the normal error-code frame builder. */
ISR_ERRCODE 8

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

    BUILD_FRAME_NOERR 128
    CALL_HANDLER

    DISCARD_FRAME

    swapgs
    iretq

/* ==========================================================================
 * load_idt(uint64_t idt_ptr)
 * ======================================================================= */
load_idt:
    /* First argument in rdi (SysV AMD64 ABI) */
    lidt [rdi]
    ret