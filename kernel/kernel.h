
#ifndef KERNEL_H
#define KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

void system_shutdown(void);
void system_reboot(void);
void kernel_main(void);
void draw_start(void);
void kernel_setup(void);
void launch_app(int app_code);

extern int owly;

#ifdef __cplusplus
}
#endif

#endif
