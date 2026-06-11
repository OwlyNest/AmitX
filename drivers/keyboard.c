#include "keyboard.h"
#include "io.h"
#include "screen.h"
#include "time.h"
#include "interrupts.h"
#include "amitx_consts.h"
#include <stdint.h>
#include <stddef.h>


#define MAX_SCANCODE 128
#define KEYBOARD_BUFFER_SIZE 256

static uint8_t key_state[MAX_SCANCODE] = {0};

static volatile unsigned char key_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint8_t buffer_head = 0;
static volatile uint8_t buffer_tail = 0;

static int shift_down = 0;
static uint8_t expecting_e0 = 0;

/* ------------------------------------------------------------------ */
/* Scancode maps — these produce ASCII for the consumer               */
/* ------------------------------------------------------------------ */
static const char scancode_map[] = {
    0,  0,    '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  /* LCtrl */
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  /* LShift */
    '\\','z','x','c','v','b','n','m',',','.','/',
    0,  /* RShift */
    '*',
    0,  /* LAlt */
    ' ',/* Space */
    0,  /* Caps lock */
};

static const char scancode_map_shift[] = {
    0,  0,    '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  /* LCtrl */
    'A','S','D','F','G','H','J','K','L',':','\"','~',
    0,  /* LShift */
    '|','Z','X','C','V','B','N','M','<','>','?',
    0,  /* RShift */
    '*',
    0,  /* LAlt */
    ' ',/* Space */
    0,  /* Caps lock */
};

/* ------------------------------------------------------------------ */
/* Ring buffer                                                        */
/* ------------------------------------------------------------------ */

static void buffer_put(unsigned char c) {
    uint8_t next = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != buffer_tail) {          /* silently drop if full */
        key_buffer[buffer_head] = c;
        buffer_head = next;
    }
}

static unsigned char buffer_get(void) {
    while (buffer_head == buffer_tail) {
        __asm__ __volatile__("sti; hlt");   // enable ints, then halt
    }
    __asm__ __volatile__("cli");
    unsigned char c = key_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    __asm__ __volatile__("sti");
    return c;
}

int keyboard_has_char(void) {
    return buffer_head != buffer_tail;
}

void keyboard_flush(void) {
    __asm__ __volatile__("cli");
    buffer_head = buffer_tail = 0;
    __asm__ __volatile__("sti");
}

unsigned char keyboard_getchar(void) {
    return buffer_get();
}

/* ------------------------------------------------------------------ */
/* IRQ1 handler                                                       */
/* ------------------------------------------------------------------ */

void keyboard_callback() {
    uint8_t scancode = inb(PORT_KBD_DATA);

    if (scancode == SC_PREFIX_E0) {
        expecting_e0 = 1;
        return;
    }

    uint8_t is_release = scancode & SC_RELEASE_BIT;
    uint8_t code       = scancode & SC_CODE_MASK;

    /* ---- Extended scancodes (arrow keys, etc.) ---- */
    if (expecting_e0) {
        if (!is_release) {
            switch (code) {
                case SC_E0_UP:
                    buffer_put(KEY_UP);
                    break;
                case SC_E0_DOWN:
                    buffer_put(KEY_DOWN);
                    break;
                case SC_E0_LEFT:
                    buffer_put(KEY_LEFT);
                    break;
                case SC_E0_RIGHT:
                    buffer_put(KEY_RIGHT);
                    break;
            }
        }
        expecting_e0 = 0;
        return;
    }

    /* ---- Shift modifiers ---- */
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_down = 1;
        return;
    }
    if (scancode == (SC_LSHIFT | SC_RELEASE_BIT) ||
        scancode == (SC_RSHIFT | SC_RELEASE_BIT)) {
        shift_down = 0;
        return;
    }

    if (code >= MAX_SCANCODE) return;

    if (is_release) {
        key_state[code] = 0;
    } else {
        if (!key_state[code]) {
            key_state[code] = 1;

            static const size_t MAP_LEN = sizeof(scancode_map) / sizeof(scancode_map[0]);
            if (code >= MAP_LEN) return;

            unsigned char c = shift_down ? scancode_map_shift[code] : scancode_map[code];
            if (!c) return;

            buffer_put(c);
        }
    }
}

void init_keyboard() {
    register_interrupt_handler(VECTOR_IRQ1, keyboard_callback);
    __asm__ __volatile__ ("sti");
}

void reset_keyboard_state() {
    for (int i = 0; i < MAX_SCANCODE; i++) key_state[i] = 0;
    keyboard_flush();
    register_interrupt_handler(VECTOR_IRQ1, keyboard_callback);
}