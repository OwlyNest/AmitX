#include "internal/phonon_types.h"
#include <arch/x86/time.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/serial.h>
#include <fs/amfs.h>
#include <gfx/compositor.h>
#include <gfx/window.h>
#include <hw/rtc.h>
#include <internal/phonon_info.h>
#include <kernel/syscall.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <screen/printk.h>
#include <screen/screen.h>
#include <stdint.h>

static syscall_func_t syscall_table[MAX_SYSCALLS] = {0};

VOID register_syscall(INT num, syscall_func_t func) {
  if (num < MAX_SYSCALLS) {
    syscall_table[num] = func;
  }
}

VOID syscall_dispatch(interrupt_frame_t *frame) {
#if ARCH_X86_64
  uint64_t num = frame->rax;
  uint64_t a1 = frame->rbx;
  uint64_t a2 = frame->rcx;
  uint64_t a3 = frame->rdx;

  if (num < MAX_SYSCALLS && syscall_table[num]) {
    /* Return value goes back in frame->eax */
    frame->rax = syscall_table[num](a1, a2, a3);
  } else {
    frame->rax = (uint64_t)-1; /* Invalid syscall */
  }
#else
  uint32_t num = frame->eax;
  uint32_t a1 = frame->ebx;
  uint32_t a2 = frame->ecx;
  uint32_t a3 = frame->edx;

  if (num < MAX_SYSCALLS && syscall_table[num]) {
    /* Return value goes back in frame->eax */
    frame->eax = syscall_table[num](a1, a2, a3);
  } else {
    frame->eax = (uint32_t)-1; /* Invalid syscall */
  }
#endif
}

INT syscall(INT num, ULONG_PTR arg1, ULONG_PTR arg2, ULONG_PTR arg3) {
  int ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
               : "memory");
  return ret;
}

/* ==========================================================================
 * Syscall implementations
 * ======================================================================= */

static ULONG_PTR _sys_exit(ULONG_PTR code, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;

  /* For v1: just hang. Later: scheduler kills the process */
  while (1) {
    asm volatile("hlt");
  }
  return code;
}

static ULONG_PTR _sys_getchar(ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a1;
  (void)a2;
  (void)a3;
  return (INT)keyboard_getchar();
}

static ULONG_PTR _sys_putchar(ULONG_PTR ch, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;
  putc((char)ch);
  return 0;
}

static ULONG_PTR _sys_read(ULONG_PTR fd, ULONG_PTR buf, ULONG_PTR len) {
  if (fd == 0) {
    /* stdin — not implemented for v1 */
    return -1;
  }
  /* For v1, fd > 2 is ignored — we only have AMFS, no fd table yet */
  (void)buf;
  (void)len;
  return -1;
}

static ULONG_PTR _sys_write(ULONG_PTR fd, ULONG_PTR buf, ULONG_PTR a3) {
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

static ULONG_PTR _sys_puts(ULONG_PTR str, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;
  const char *s = (const char *)str;
  printk(s);
  return strlen(s);
}

static ULONG_PTR _sys_gets(ULONG_PTR buf, ULONG_PTR max, ULONG_PTR a3) {
  (void)a3;
  char *b = (char *)buf;
  ULONG_PTR i = 0;
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

static ULONG_PTR _sys_reset_disk(ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a1;
  (void)a2;
  (void)a3;
  /* Flush any pending writes — AMFS writes immediately, so no-op */
  return 0;
}

static ULONG_PTR _sys_unlink(ULONG_PTR path, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;
  return amfs_delete((const char *)path);
}

static ULONG_PTR _sys_create(ULONG_PTR path, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;
  return amfs_create((const char *)path);
}

static ULONG_PTR _sys_getdate(ULONG_PTR buf, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;
  rtc_time_t *t = (rtc_time_t *)buf;
  rtc_read(t);
  return 0;
}

static ULONG_PTR _sys_gettime(ULONG_PTR buf, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;
  rtc_time_t *t = (rtc_time_t *)buf;
  rtc_read(t);
  return 0;
}

static ULONG_PTR _sys_version(ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a1;
  (void)a2;
  (void)a3;
  return (ULONG_PTR)PHONON_VERSION;
}

static ULONG_PTR _sys_malloc(ULONG_PTR size, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;
  return (ULONG_PTR)kmalloc(size);
}

static ULONG_PTR _sys_free(ULONG_PTR ptr, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;
  kfree((void *)ptr);
  return 0;
}

static ULONG_PTR _win_create(ULONG_PTR w, ULONG_PTR h, ULONG_PTR flags) {
  int x = ((INT)fb.back.width - (INT)w) / 2;
  int y = ((INT)fb.back.height - (INT)h) / 2;
  return window_create(x, y, w, h, "", flags);
}

static ULONG_PTR _win_destroy(window_handle_t handle, ULONG_PTR a2,
                              ULONG_PTR a3) {
  (void)a2;
  (void)a3;
  window_destroy(handle);
  return 0;
}

static ULONG_PTR _win_show(window_handle_t handle, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;

  window_t *win = window_get(handle);
  if (!win)
    return -1;

  win->state |= WIN_STATE_VISIBLE;

  return 0;
}

static ULONG_PTR _win_hide(window_handle_t handle, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a2;
  (void)a3;

  window_t *win = window_get(handle);
  if (!win)
    return -1;

  win->state &= ~WIN_STATE_VISIBLE;
  return 0;
}

static ULONG_PTR _win_set_title(window_handle_t handle, ULONG_PTR ptr,
                                ULONG_PTR a3) {
  (void)a3;
  const char *title = (char *)ptr;

  window_set_title(handle, title);
  return 0;
}

static ULONG_PTR _win_set_active(window_handle_t handle, ULONG_PTR a2,
                                 ULONG_PTR a3) {
  (void)a2;
  (void)a3;

  window_t *win = window_get(handle);
  if (!win)
    return -1;

  for (int i = 0; i < WIN_MAX_WINDOWS; i++) {
    window_t *w = window_get(i);
    if (!w)
      continue;
    w->state &= ~WIN_STATE_FOCUSED;
  }

  win->state |= WIN_STATE_FOCUSED;

  return 0;
}

static ULONG_PTR _win_get_active(ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a1;
  (void)a2;
  (void)a3;

  for (int i = 0; i < WIN_MAX_WINDOWS; i++) {
    window_t *win = window_get(i);
    if (!win)
      continue;
    if (win->state & WIN_STATE_FOCUSED) {
      return i;
    }
  }
  return -1;
}

static ULONG_PTR _win_get_surface(ULONG_PTR handle, ULONG_PTR out_ptr,
                                  ULONG_PTR a3) {
  (void)a3;
  window_t *win = window_get(handle);
  if (!win)
    return -1;

  ULONG_PTR *out = (ULONG_PTR *)out_ptr;
  *out = (ULONG_PTR)win->surface.pixels;
  return 0;
}

static ULONG_PTR _win_get_pitch(window_handle_t handle, ULONG_PTR a2,
                                ULONG_PTR a3) {
  (void)a2;
  (void)a3;

  window_t *win = window_get(handle);
  if (!win)
    return -1;

  return (INT)win->surface.pitch;
}

static ULONG_PTR _win_get_dims(ULONG_PTR handle, ULONG_PTR w_ptr,
                               ULONG_PTR h_ptr) {
  window_t *win = window_get(handle);
  if (!win)
    return -1;

  ULONG_PTR *w = (ULONG_PTR *)w_ptr;
  ULONG_PTR *h = (ULONG_PTR *)h_ptr;

  if (w)
    *w = win->surface.width;
  if (h)
    *h = win->surface.height;

  return 0;
}

static ULONG_PTR _win_clear(window_handle_t handle, ULONG_PTR color,
                            ULONG_PTR a3) {
  (void)a3;

  window_clear(handle, color);

  return 0;
}

static ULONG_PTR _win_present(window_handle_t handle, ULONG_PTR a2,
                              ULONG_PTR a3) {
  (void)a2;
  (void)a3;

  compositor_blit_window(window_get(handle));

  return 0;
}

static ULONG_PTR _mouse_pos(ULONG_PTR x_ptr, ULONG_PTR y_ptr, ULONG_PTR a3) {
  (void)a3;

  ULONG_PTR *x = (ULONG_PTR *)x_ptr;
  ULONG_PTR *y = (ULONG_PTR *)y_ptr;

  int tx, ty;

  get_mouse_position(&tx, &ty);

  *x = (ULONG_PTR)tx;
  *y = (ULONG_PTR)ty;

  return 0;
}

static ULONG_PTR _mouse_buttons(ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3) {
  (void)a1;
  (void)a2;
  (void)a3;
  return mouse_button_state();
}

void syscall_init(void) {
  memset(syscall_table, 0, sizeof(syscall_table));
  register_syscall(SYS_EXIT, _sys_exit);
  register_syscall(SYS_GETCHAR, _sys_getchar);
  register_syscall(SYS_PUTCHAR, _sys_putchar);
  register_syscall(SYS_READ, _sys_read);
  register_syscall(SYS_WRITE, _sys_write);
  register_syscall(SYS_PUTS, _sys_puts);
  register_syscall(SYS_GETS, _sys_gets);
  register_syscall(SYS_RESET_DISK, _sys_reset_disk);
  register_syscall(SYS_UNLINK, _sys_unlink);
  register_syscall(SYS_CREATE, _sys_create);
  register_syscall(SYS_GETDATE, _sys_getdate);
  register_syscall(SYS_GETTIME, _sys_gettime);
  register_syscall(SYS_VERSION, _sys_version);
  register_syscall(SYS_MALLOC, _sys_malloc);
  register_syscall(SYS_FREE, _sys_free);

  register_syscall(WIN_CREATE, _win_create);
  register_syscall(WIN_DESTROY, _win_destroy);
  register_syscall(WIN_SHOW, _win_show);
  register_syscall(WIN_HIDE, _win_hide);
  register_syscall(WIN_SET_TITLE, _win_set_title);
  register_syscall(WIN_SET_ACTIVE, _win_set_active);
  register_syscall(WIN_GET_ACTIVE, _win_get_active);
  register_syscall(WIN_GET_SURFACE, _win_get_surface);
  register_syscall(WIN_GET_PITCH, _win_get_pitch);
  register_syscall(WIN_GET_DIMS, _win_get_dims);
  register_syscall(WIN_CLEAR, _win_clear);
  register_syscall(WIN_PRESENT, _win_present);
  register_syscall(MOUSE_POS, _mouse_pos);
  register_syscall(MOUSE_BUTTONS, _mouse_buttons);
}