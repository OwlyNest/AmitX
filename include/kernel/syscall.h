
#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <arch/x86/interrupts.h>

#define MAX_SYSCALLS 256

/* v1.0 syscall numbers — DOS 1.0 inspired */
#define SYS_EXIT        0x00
#define SYS_GETCHAR     0x01
#define SYS_PUTCHAR     0x02
#define SYS_READ        0x03
#define SYS_WRITE       0x04
#define SYS_PUTS        0x09
#define SYS_GETS        0x0A
#define SYS_RESET_DISK  0x0D
#define SYS_SET_DRIVE   0x0E
#define SYS_OPEN        0x0F
#define SYS_CLOSE       0x10
#define SYS_UNLINK      0x13
#define SYS_CREAT       0x16
#define SYS_SEEK        0x1A
#define SYS_GETDATE     0x2A
#define SYS_GETTIME     0x2C
#define SYS_VERSION     0x30
#define SYS_MALLOC      0x48
#define SYS_FREE        0x49

typedef int (*syscall_func_t)(uint32_t, uint32_t, uint32_t);

void syscall_dispatch(interrupt_frame_t *frame);               // Called from isr128
void register_syscall(int num, syscall_func_t func);
void syscall_init();

int syscall(int num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif
