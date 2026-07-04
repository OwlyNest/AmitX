# arch/x86/context_switch.S - [Enter description]
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

# ==============================
# === Globals ===
.globl context_switch

# ==============================
# === Externs ===

# ==============================
# === Data ===
.data

# ==============================
# === Code ===
.text

/* void context_switch(task_t *old, task_t *new) */
context_switch:
    push ebp
    push ebx
    push esi
    push edi

    # get pointers
    mov eax, [esp + 20] # task_t *old
    mov edx, [esp + 24] # task_t *new

    mov [eax + TASK_ESP], esp

    mov esp, [edx + TASK_ESP]

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret


# ==============================
# === Helpers ===

# ==============================
# === Tests ===

# ==============================
# === Buffers ===
.bss

# ==============================
# === Read-Only Data ===
.section     .rodata

# ==============================
# === Messages ===
