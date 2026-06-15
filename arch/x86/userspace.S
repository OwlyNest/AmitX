# arch/x86/userspace.S - [Enter description]
# Author:   amity
# Date:     Fri Jun 12 08:52:12 2026
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
.global usermode_jump

# ==============================
# === Globals ===

# ==============================
# === Externs ===

# ==============================
# === Data ===

# ==============================
# === Code ===
/* void usermode_jump(uint32_t eip, uint32_t esp) */
/* Stack: [esp+4] = eip, [esp+8] = esp */
usermode_jump:
    cli
    mov eax, [esp + 8]       /* user ESP */
    mov ecx, [esp + 4]       /* user EIP */

    /* Build ring 3 stack frame for iret */
    push 0x23                /* SS = user data | RPL=3 (0x20 | 3) */
    push eax                 /* ESP */
    or dword ptr [esp], 0x200
    pushf                    /* EFLAGS */
    push 0x1B                /* CS = user code | RPL=3 (0x18 | 3) */
    push ecx                 /* EIP */

    /* Set up user segments for after iret (though iret pops them) */
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    iret
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
