# arch/x86/64/context_switch.S - [Enter description]
# Author:   amity
# Date:     Mon Aug 31 15:18:44 2026
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

/* --- Offsets into task_t (must match task.h layout) --- */
/* task_t.context starts at offset after: tid, pid, name[32], state, class,
 * base_prio, cur_prio, quantum, quantum_max, sleep_until
 *
 * Using cpu_context_t from task.h:
 *   rsp, rbp, rbx, r12, r13, r14, r15, rdi, rsi, rflags, rip
 */
#define TASK_OFF_CTX_RSP     56
#define TASK_OFF_CTX_RBP     64
#define TASK_OFF_CTX_RBX     72
#define TASK_OFF_CTX_R12     80
#define TASK_OFF_CTX_R13     88
#define TASK_OFF_CTX_R14     96
#define TASK_OFF_CTX_R15    104
#define TASK_OFF_CTX_RDI    112
#define TASK_OFF_CTX_RSI    120
#define TASK_OFF_CTX_RFLAGS 128
#define TASK_OFF_CTX_RIP    136

/* --- Globals --- */
.globl context_switch

/* --- Code --- */
.text

/* ======================================================================== *
 * context_switch                                                           *
 *                                                                          *
 * void context_switch(task_t *old, task_t *new)                            *
 *                                                                          *
 * SysV AMD64 ABI: first arg in rdi, second arg in rsi                      *
 *                                                                          *
 * Saves callee-saved registers (rbx, rbp, r12-r15) and rflags              *
 * on the current kernel stack, stores rsp in old->context.rsp,             *
 * then loads new->context.rsp and restores.                                *
 *                                                                          *
 * On first entry, the stack was set up by task_create so that              *
 * pops + ret land at task_trampoline.                                      *
 * ======================================================================== */
context_switch:
    /* Save old context on old stack */
    push    rbp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    push    rdi
    push    rsi
    pushfq

    /* Save old RSP into task struct (rdi = old task) */
    mov     [rdi + TASK_OFF_CTX_RSP], rsp

    /* Load new RSP from task struct (rsi = new task) */
    mov     rsp, [rsi + TASK_OFF_CTX_RSP]

    /* Restore new context from new stack */
    popfq
    pop     rsi
    pop     rdi
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp

    ret