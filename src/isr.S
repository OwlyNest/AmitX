
.intel_syntax noprefix

.global load_idt

# Export all ISR symbols
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
.global isr44
.global isr128

.extern isr_handler
.extern isr0_handler
.extern syscall_handler

# ISR with no error code
.macro ISR_NO_ERRCODE num
isr\num:
    cli
    pusha
    push 0              # Fake error code
    push \num           # Interrupt number
    call isr_handler
    add esp, 8
    popa
    sti
    iret
.endm

# ISR with error code pushed by CPU
.macro ISR_ERRCODE num
isr\num:
    cli
    pusha
    push \num           # Interrupt number (error code is already on stack)
    call isr_handler
    add esp, 8
    popa
    sti
    iret
.endm

# Generate all ISRs 0–31
ISR_NO_ERRCODE 0
ISR_NO_ERRCODE 1
ISR_NO_ERRCODE 2
ISR_NO_ERRCODE 3
ISR_NO_ERRCODE 4
ISR_NO_ERRCODE 5
ISR_NO_ERRCODE 6
ISR_NO_ERRCODE 7
ISR_ERRCODE    8
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

# IRQ0 (timer)
isr32:
    pusha
    push 0              
    push 32
    call isr_handler
    add esp, 8
    popa
    iret

# IRQ1 (keyboard)
isr33:
    pusha
    push 0
    push 33
    call isr_handler
    add esp, 8
    popa
    iret

# IRQ12 (PS/2 Mouse)
isr44:
    pusha
    push 0
    push 44
    call isr_handler
    add esp, 8
    popa
    iret

# Syscall (int 0x80)
isr128:
    pusha
    call syscall_handler
    popa
    iret

# Load IDT helper
load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    ret
