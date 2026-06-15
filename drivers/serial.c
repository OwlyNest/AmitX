/*
	* drivers/serial.c - [Enter description]
	* Author:   amity
	* Date:     Wed Jun 10 15:47:55 2026
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
#include "drivers/serial.h"
#include "arch/x86/io.h"
#include "internal/amitx_consts.h"
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

void serial_init(void) {
	outb(PORT_SERIAL + 1, 0x00); // Disable all interrupts
	outb(PORT_SERIAL + 3, 0x80); // Enable DLAB (set bound rate divisor)
	outb(PORT_SERIAL + 0, 0x03); // Set divisor to 3 (38400 bound)
	outb(PORT_SERIAL + 1, 0x00);  /* High byte of divisor */
    outb(PORT_SERIAL + 3, 0x03);  /* 8 bits, no parity, one stop bit */
    outb(PORT_SERIAL + 2, 0xC7);  /* Enable FIFO, clear them, 14-byte threshold */
    outb(PORT_SERIAL + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

static int serial_transmit_empty(void) {
	return inb(PORT_SERIAL + 5) & 0x20;
}

void serial_putc(char c) {
	while (!serial_transmit_empty());
	outb(PORT_SERIAL, c);
}

void serial_puts(const char *s) {
	while (*s) {
		serial_putc(*s++);
	}
}