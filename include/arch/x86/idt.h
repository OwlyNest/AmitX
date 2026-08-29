
#ifndef __ARCH_X86_IDT_H__
#define __ARCH_X86_IDT_H__
#include <stdint.h>

void idt_set_gate(int num, DWORD base, WORD sel, BYTE flags);

struct IDTEntry {
    WORD base_lo;
    WORD sel;
    BYTE  always0;
    BYTE  flags;
    WORD base_hi;
} __attribute__((packed));

struct IDTPointer {
    WORD limit;
    DWORD base;
} __attribute__((packed));

#endif
