/*
	* drivers/keyboard.c - PS/2 keyboard driver (scancode set 1)
	* Author:   amity
	* Date:     Sat Jun 20 22:53:57 2026
	* Copyright © 2026 OwlyNest
*/

/* --- Styling Instructions ---
	* Encoding:                      UTF-8, Unix line endings
	* Text font:                     Monospace
	* Line width:                    Max 80 characters
	* Indentation:                   Use 4 spaces
	* Brace style:                   Same line as control statement
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include <drivers/keyboard.h>
#include <arch/x86/io.h>
#include <arch/x86/interrupts.h>
#include <internal/amitx_consts.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <screen/printk.h>
#include <stdint.h>
#include <stddef.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

#define MAX_SCANCODE 128
#define KEYBOARD_BUFFER_SIZE 256

static uint8_t key_state[MAX_SCANCODE] = { 0 };

static volatile unsigned char key_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint8_t buffer_head = 0;
static volatile uint8_t buffer_tail = 0;
static volatile uint32_t buffer_overflows = 0;

static int shift_down = 0;
static int ctrl_down = 0;
static uint8_t expecting_e0 = 0;

/* ------------------------------------------------------------------ */
/* Scancode maps — indexed by scancode (set 1).                       */
/* Index 0 is unused (no scancode 0).                               */
/* ------------------------------------------------------------------ */
static const char scancode_map[] = {
    0,        /* 0x00 — unused */
    KEY_ESC,  /* 0x01 — ESC */
    '1','2','3','4','5','6','7','8','9','0','-','=','\b',
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
    0,        /* 0x00 — unused */
    KEY_ESC,  /* 0x01 — ESC */
    '!','@','#','$','%','^','&','*','(',')','_','+','\b',
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

static const size_t MAP_LEN = sizeof(scancode_map) / sizeof(scancode_map[0]);

/* --- Prototypes ---*/
static void buffer_put(unsigned char c);
static unsigned char buffer_get(void);
static int keyboard_callback(interrupt_frame_t *frame);

/* --- Functions ---*/

/* ==========================================================================
 * Ring buffer — called from IRQ context with interrupts already disabled
 * ======================================================================= */
static void buffer_put(unsigned char c) {
    uint8_t next = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;

    if (next == buffer_tail) {
        buffer_overflows++;
        return;
    }

    key_buffer[buffer_head] = c;
    buffer_head = next;
}

/* ==========================================================================
 * Blocking read with lost-wake-up protection.
 *
 * We check the empty condition with interrupts disabled.
 * If empty, we atomically enable interrupts and halt (sti; hlt).
 * The next IRQ will wake us. We disable interrupts again before
 * re-checking, because another CPU (or a nested IRQ) could race.
 * ======================================================================= */
static unsigned char buffer_get(void) {
    unsigned char c;

    __asm__ __volatile__("cli");
    while (buffer_head == buffer_tail) {
        __asm__ __volatile__("sti; hlt; cli");
    }
    c = key_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    __asm__ __volatile__("sti");

    return c;
}

/* ==========================================================================
 * Non-blocking check
 * ======================================================================= */
int keyboard_has_char(void) {
    int has;

    __asm__ __volatile__("cli");
    has = (buffer_head != buffer_tail);
    __asm__ __volatile__("sti");

    return has;
}

/* ==========================================================================
 * Clear buffer
 * ======================================================================= */
void keyboard_flush(void) {
    __asm__ __volatile__("cli");
    buffer_head = buffer_tail = 0;
    __asm__ __volatile__("sti");
}

/* ==========================================================================
 * Blocking character read
 * ======================================================================= */
unsigned char keyboard_getchar(void) {
    return buffer_get();
}

int keyboard_poll(unsigned char *c) {
    __asm__ __volatile__("cli");

    if (buffer_head == buffer_tail) {
        __asm__ __volatile__("sti");
        return 0;
    }

    *c = key_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;

    __asm__ __volatile__("sti");
    return 1;
}

/* ==========================================================================
 * IRQ1 handler
 * ======================================================================= */
static int keyboard_callback(interrupt_frame_t *frame) {
    (void)frame;
    uint8_t scancode = inb(PORT_KBD_DATA);

    if (scancode == SC_PREFIX_E0) {
        expecting_e0 = 1;
        return 1;
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
            default:
                /* Unknown extended key, ignore */
                break;
            }
        }
        expecting_e0 = 0;
        return 1;
    }

    /* ---- Shift modifiers ---- */
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_down = 1;
        return 1;
    }
    if (scancode == (SC_LSHIFT | SC_RELEASE_BIT) ||
        scancode == (SC_RSHIFT | SC_RELEASE_BIT)) {
        shift_down = 0;
        return 1;
    }

    /* --- Ctrl modifiers --- */
    if (scancode == SC_LCTRL) {
        ctrl_down = 1;
        return 1;
    }

    if (scancode == (SC_LCTRL | SC_RELEASE_BIT)) {
        ctrl_down = 0;
        return 1;
    }

    if (code >= MAX_SCANCODE)
        return 1;

    if (is_release) {
        key_state[code] = 0;
        return 1;
    }

    /* ---- Make code: only buffer on first press, ignore autorepeat ---- */
    if (!key_state[code]) {
        key_state[code] = 1;

        if (code >= MAP_LEN)
            return 1;

        unsigned char c = shift_down
            ? scancode_map_shift[code]
            : scancode_map[code];

        if (ctrl_down) {
            if (c >= 'a' && c <= 'z')
                c = c - 'a' + 1;
            else if (c >= 'A' && c <= 'Z')
                c = c - 'A' + 1;
        }

        if (!c)
            return 1;

        buffer_put(c);
    }
    return 1;
}

/* ==========================================================================
 * KScope init
 * ======================================================================= */
static int init_keyboard(void) {
    register_interrupt_handler(VECTOR_IRQ1, keyboard_callback);

    /* KScope guarantees PIC and IDT are initialized before this runs */
    __asm__ __volatile__("sti");
    pic_unmask_irq(1);

    return 0;
}

kscope_node_t keyboard_node = {
    .name = "PS/2-keyboard",
    .id = 0x0005,
    .class = KSCOPE_CLASS_DRIVER,
    .subclass = KSCOPE_SUBCLASS_DRIVER_KEYBOARD,
    .requires = (kscope_node_t *[]){ &x86_pic_node, &x86_idt_node },
    .require_count = 2,
    .provides = (const char *[]){"drivers.keyboard", "irq.1"},
    .provide_count = 2,
    .init = init_keyboard
};

/* ==========================================================================
 * Reset driver state (does not re-register handler)
 * ======================================================================= */
void reset_keyboard_state(void) {
    for (int i = 0; i < MAX_SCANCODE; i++)
        key_state[i] = 0;

    keyboard_flush();
    expecting_e0 = 0;
    shift_down = 0;
}