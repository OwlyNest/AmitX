
#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define MAX_SYSCALLS 32

// Syscall IDs
#define SYSCALL_WRITE       0
#define SYSCALL_TIME        1
#define SYSCALL_CLEAR       2
#define SYSCALL_CURSOR      3
#define SYSCALL_DRAW_LOGO   4
#define SYSCALL_GETCHAR     5
#define SYSCALL_PUTCHAR     6
#define SYSCALL_SLEEP       7

typedef int (*syscall_func_t)(uint32_t, uint32_t, uint32_t);

void syscall_handler();                      // Called from isr128
void register_syscall(int num, syscall_func_t func);
void syscall_init();

int syscall(int num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif
