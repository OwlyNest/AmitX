
#include <arch/x86/idt.h>
#include <stdint.h>
#include <internal/amitx_consts.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>

// Ignore intellisense, these exist in the Assembly code
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void isr32();
extern void isr33();
extern void isr44();
extern void isr128();
extern void load_idt(uint32_t);

#define IDT_ENTRIES 256
static struct IDTEntry idt[IDT_ENTRIES];
static struct IDTPointer idt_ptr;

static void (*const exception_isrs[])(void) = {
    isr0,  isr1,  isr2,  isr3,
    isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11,
    isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19,
    isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27,
    isr28, isr29, isr30, isr31
};

void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;

    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}
static int idt_install() {
    idt_ptr.limit = sizeof(struct IDTEntry) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uint32_t)&idt;

    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint32_t)exception_isrs[i], GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
    }

    idt_set_gate(32,  (uint32_t)isr32,  GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
    idt_set_gate(33,  (uint32_t)isr33,  GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
    idt_set_gate(44,  (uint32_t)isr44,  GDT_SEL_KERNEL_CODE, IDT_FLAGS_KERNEL);
    idt_set_gate(128, (uint32_t)isr128, GDT_SEL_KERNEL_CODE, IDT_FLAGS_USER  );

    load_idt((uint32_t)&idt_ptr);
    __asm__ __volatile__("sti");
    return 0;
}


kscope_node_t x86_idt_node = {
    .name = "x86-idt",
    .id = 0x0003,
    .class = KSCOPE_CLASS_CORE,
    .subclass = KSCOPE_SUBCLASS_CORE_IDT,
    .requires = (kscope_node_t*[]){ &x86_gdt_node, &x86_pic_node },
    .require_count = 2,
    .provides = (const char*[]){"cpu.interrupts", "cpu.irq"},
	.provide_count = 2,
    .init = idt_install,

};