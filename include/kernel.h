
#ifndef KERNEL_H
#define KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

void kernel_main(void);
void draw_start(void);
void kernel_setup(void);
void qemu_exit(int code);
void launch_app(int app_code);

#ifdef __cplusplus
}
#endif

#endif
