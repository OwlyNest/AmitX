
#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Special non-ASCII key codes (high bit set, won't collide with printable ASCII) */
#define KEY_UP     0x80
#define KEY_DOWN   0x81
#define KEY_LEFT   0x82
#define KEY_RIGHT  0x83

void init_keyboard();
void keyboard_callback();
void reset_keyboard_state();

unsigned char keyboard_getchar();
int  keyboard_has_char(void);     /* non-blocking check */
void keyboard_flush(void);        /* clear buffer */

#endif
