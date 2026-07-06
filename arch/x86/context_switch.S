# arch/x86/context_switch.S - Context switch
# Author:   amity
# Date:     Sun Jul  5 00:59:08 2026
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

# ==============================
# === Macros ===
.intel_syntax noprefix
# Offsets into task_t (must match task.h layout)
#define TASK_OFF_ESP       56
#define TASK_OFF_EBP       60
#define TASK_OFF_EBX       64
#define TASK_OFF_ESI       68
#define TASK_OFF_EDI       72
#define TASK_OFF_EFLAGS    76

# ==============================
# === Globals ===
.globl context_switch

# ==============================
# === Code ===
.text

# ==============================
# === context_switch ===
#
# void context_switch(task_t *old, task_t *new)
#
# Saves callee-saved registers and eflags of the current task
# on its kernel stack, stores the stack pointer in old->esp,
# then loads new->esp and restores the new task's registers.
#
# On first entry to a new task, the stack was set up by task_create
# so that the 5 pops followed by ret land at the task entry point.
# ==============================
context_switch:
    mov     eax, [esp + 4]          # old task
    mov     edx, [esp + 8]          # new task

    # Save old context on old stack
    push    ebp
    push    ebx
    push    esi
    push    edi
    pushfd

    # Save old ESP into task struct
    mov     [eax + TASK_OFF_ESP], esp

    # Load new ESP from task struct
    mov     esp, [edx + TASK_OFF_ESP]

    # Restore new context from new stack
    popfd
    pop     edi
    pop     esi
    pop     ebx
    pop     ebp

    ret

# ==============================
# === Buffers ===
.bss

# ==============================
# === Read-Only Data ===
.section     .rodata

# ==============================
# === Messages ===