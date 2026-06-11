#include "syscall.h"
#include "screen.h"
#include "time.h"
#include "keyboard.h"
#include "logo.h"

static syscall_func_t syscall_table[MAX_SYSCALLS] = { 0 };

void register_syscall(int num, syscall_func_t func) {
    if (num < MAX_SYSCALLS) {
        syscall_table[num] = func;
    }
}

void syscall_handler() {
    uint32_t num, a1, a2, a3;

    asm volatile (
        "" : "=a"(num), "=b"(a1), "=c"(a2), "=d"(a3)
    );

    if (num < MAX_SYSCALLS && syscall_table[num]) {
        syscall_table[num](a1, a2, a3);
    } else {
        puts("Invalid syscall\n");
    }
}

int syscall(int num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
    );
    return ret;
}

static int syscall_write(uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    puts((const char*)a1);
    return 0;
}

static int syscall_time(uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    extern int tick_count;
    return tick_count;
}

static int syscall_clear(uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    clear();
    return 0;
}

static int syscall_cursor(uint32_t x, uint32_t y, uint32_t unused) {
    (void)unused;
    move_cursor(x, y);
    return 0;
}

static int syscall_draw_logo(uint32_t version, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    draw_logo((int)version);
    return 0;
}

static int syscall_getchar(uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    return (int)keyboard_getchar();
}

static int syscall_putchar(uint32_t ch, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    putc((char)ch);
    return 0;
}

static int syscall_sleep(uint32_t ticks, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    sleep(ticks);
    return 0;
}

void syscall_init() {
    syscall_table[SYSCALL_WRITE]     = syscall_write;
    syscall_table[SYSCALL_TIME]      = syscall_time;
    syscall_table[SYSCALL_CLEAR]     = syscall_clear;
    syscall_table[SYSCALL_CURSOR]    = syscall_cursor;
    syscall_table[SYSCALL_DRAW_LOGO] = syscall_draw_logo;
    syscall_table[SYSCALL_GETCHAR]   = syscall_getchar;
    syscall_table[SYSCALL_PUTCHAR]   = syscall_putchar;
    syscall_table[SYSCALL_SLEEP]     = syscall_sleep;
}