#include <kernel/syscall.h>
#include <drivers/serial.h>
#include <screen/screen.h>
#include <screen/printk.h>
#include <arch/x86/time.h>
#include <internal/amitx_info.h>
#include <drivers/keyboard.h>
#include <fs/amfs.h>
#include <hw/rtc.h>
#include <mm/heap.h>
#include <lib/string.h>

static syscall_func_t syscall_table[MAX_SYSCALLS] = { 0 };

void register_syscall(int num, syscall_func_t func) {
    if (num < MAX_SYSCALLS) {
        syscall_table[num] = func;
    }
}

void syscall_dispatch(interrupt_frame_t *frame) {
    uint32_t num = frame->eax;
    uint32_t a1  = frame->ebx;
    uint32_t a2  = frame->ecx;
    uint32_t a3  = frame->edx;

    if (num < MAX_SYSCALLS && syscall_table[num]) {
        /* Return value goes back in frame->eax */
        frame->eax = syscall_table[num](a1, a2, a3);
    } else {
        frame->eax = (uint32_t)-1;  /* Invalid syscall */
    }
}

int syscall(int num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

/* ==========================================================================
 * Syscall implementations
 * ======================================================================= */

static int _sys_exit(uint32_t code, uint32_t a2, uint32_t a3) {
    (void)a2;
    (void)a3;

    /* For v1: just hang. Later: scheduler kills the process */
    while (1) {
        asm volatile ("hlt");
    }
    return code;
}

static int _sys_getchar(uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    return (int)keyboard_getchar();
}

static int _sys_putchar(uint32_t ch, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    putc((char)ch);
    return 0;
}

static int _sys_read(uint32_t fd, uint32_t buf, uint32_t len) {
    if (fd == 0) {
        /* stdin — not implemented for v1 */
        return -1;
    }
    /* For v1, fd > 2 is ignored — we only have AMFS, no fd table yet */
    (void)buf; (void)len;
    return -1;
}

static int _sys_write(uint32_t fd, uint32_t buf, uint32_t a3) {
    (void)a3;
    const char *str = (const char *)buf;
    if (fd == 1 || fd == 2) {
        /* stdout / stderr */
        serial_puts_default(str);
        /* Also to screen if in graphics mode */
        puts(str);
        return strlen(str);
    }
    return -1;
}

static int _sys_puts(uint32_t str, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    const char *s = (const char *)str;
    printk(s);
    return strlen(s);
}

static int _sys_gets(uint32_t buf, uint32_t max, uint32_t a3) {
    (void)a3;
    char *b = (char *)buf;
    uint32_t i = 0;
    while (i < max - 1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            b[i] = '\0';
            return i;
        }
        if (c == '\b') {
            if (i > 0) {
                i--;
                putc('\b');
            }
            continue;
        }
        b[i++] = c;
        putc(c);
    }
    b[i] = '\0';
    return i;
}

static int _sys_reset_disk(uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    /* Flush any pending writes — AMFS writes immediately, so no-op */
    return 0;
}

static int _sys_unlink(uint32_t path, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    return amfs_delete((const char *)path);
}

static int _sys_create(uint32_t path, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    return amfs_create((const char *)path);
}

static int _sys_getdate(uint32_t buf, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    rtc_time_t *t = (rtc_time_t *)buf;
    rtc_read(t);
    return 0;
}

static int _sys_gettime(uint32_t buf, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    rtc_time_t *t = (rtc_time_t *)buf;
    rtc_read(t);
    return 0;
}

static int _sys_version(uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    return (int)AMITX_VERSION;
}

static int _sys_malloc(uint32_t size, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    return (int)malloc(size);
}

static int _sys_free(uint32_t ptr, uint32_t a2, uint32_t a3) {
    (void)a2; (void)a3;
    free((void *)ptr);
    return 0;
}

void syscall_init(void) {
    memset(syscall_table, 0, sizeof(syscall_table));
    register_syscall(SYS_EXIT,       _sys_exit);
    register_syscall(SYS_GETCHAR,    _sys_getchar);
    register_syscall(SYS_PUTCHAR,    _sys_putchar);
    register_syscall(SYS_READ,       _sys_read);
    register_syscall(SYS_WRITE,      _sys_write);
    register_syscall(SYS_PUTS,       _sys_puts);
    register_syscall(SYS_GETS,       _sys_gets);
    register_syscall(SYS_RESET_DISK, _sys_reset_disk);
    register_syscall(SYS_UNLINK,     _sys_unlink);
    register_syscall(SYS_CREAT,      _sys_create);
    register_syscall(SYS_GETDATE,    _sys_getdate);
    register_syscall(SYS_GETTIME,    _sys_gettime);
    register_syscall(SYS_VERSION,    _sys_version);
    register_syscall(SYS_MALLOC,     _sys_malloc);
    register_syscall(SYS_FREE,       _sys_free);
}