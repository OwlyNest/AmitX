
#include "keyboard.h"
#include "io.h"
#include "screen.h"
#include "kernel.h"
#include "time.h"
#include "interrupts.h"
#include "idt.h"
#include <stdint.h>

extern int menu;
int load_cyclone = 0;

#define MAX_SCANCODE 128
static uint8_t key_state[MAX_SCANCODE] = {0};
static char last_char = 0;

int POINTER = 0;
static int shift_down = 0;

static const char scancode_map[] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b', // Backspace
    '\t', // Tab
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',         // Enter
    0,    // Control
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,    // Left shift
    '\\','z','x','c','v','b','n','m',',','.','/',
    0,    // Right shift
    '*',
    0,    // Alt
    ' ',  // Spacebar
    0,    // Caps lock
    // And so on...
};

static const char scancode_map_shift[] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b', // Backspace
    '\t', // Tab
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',         // Enter
    0,    // Control
    'A','S','D','F','G','H','J','K','L',':','\"','~',
    '0',    // Left shift
    '\\','Z','X','C','V','B','N','M','<','>','?',
    0,    // Right shift
    '*',
    0,    // Alt
    ' ',  // Spacebar
    0,    // Caps lock
    // And so on...
};

void decide() {
    menu = 0;
    clear();
    if (POINTER == 0) {
        puts("Launching Perch!\n");
        sleep(1);
        launch_app(1);
    } else if (POINTER == 1) {
        puts("Launching Owly!\n");
        sleep(1);
        launch_app(2);
    } else if (POINTER == 3) {
        puts("Exiting AmitX Kernel");
        for (int i = 0; i < 3; i++) {
            sleep(1);
            puts(".");
        }
        sleep(1);
        qemu_exit(0);
    } else if (POINTER == 2) {
        load_cyclone = 1;
        kernel_main();
    } else {
        kernel_main();
    }
}

void keyboard_callback() {
    uint8_t scancode = inb(0x60);

    if (scancode == 0x2A || scancode == 0x36) {
        shift_down = 1;
        return;
    } else if (scancode == 0xAA || scancode == 0xB6) {
        shift_down = 0;
        return;
    }

    uint8_t is_release = scancode & 0x80;
    uint8_t code = scancode & 0x7F;

    if (code >= MAX_SCANCODE) return;

    if (is_release) {
        key_state[code] = 0; // released
    } else {
        if (!key_state[code]) {
            key_state[code] = 1; // pressed

            char c = shift_down ? scancode_map_shift[code] : scancode_map[code];
            last_char = c;

            // Handle special logic
            if (menu) {
                if (c == 's') {
                    if (POINTER < 4) POINTER++;
                    draw_start();
                } else if (c == 'w') {
                    if (POINTER > 0) POINTER--;
                    draw_start();
                } else if (c == '\n') {
                    decide();
                    
                }
            }
        }
    }
}

void init_keyboard() {
    register_interrupt_handler(33, keyboard_callback); // IRQ1
    __asm__ __volatile__ ("sti");
}

void reset_keyboard_state() {
    // Clear key state and reset the last_char buffer
    for (int i = 0; i < MAX_SCANCODE; i++) key_state[i] = 0;
    last_char = 0;

    // Re-register IRQ1 handler — just in case
    register_interrupt_handler(33, keyboard_callback);
}

char keyboard_getchar() {
    char c = 0;

    // Wait for a new character from IRQ handler
    while (!last_char) {
        __asm__ __volatile__("hlt"); // Wait for interrupt
    }

    c = last_char;
    last_char = 0;
    return c;
}
