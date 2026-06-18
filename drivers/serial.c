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
#include <drivers/serial.h>
#include <arch/x86/io.h>
#include <internal/amitx_consts.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
serial_port_t serial_com1;
/* --- Prototypes ---*/

/* --- Functions ---*/

static int serial_detect(uint16_t base) {
	uint8_t tmp;

	outb(base + 7, 0x5A);
	tmp = inb(base + 7);
	if (tmp != 0x5A) {
		return -1;
	}

	outb(base + 7, 0xA5);
	tmp = inb(base + 7);
	if (tmp != 0xA5) {
		return -1;
	}
	return 0;
}

static int serial_init(void) {
	serial_port_t *port = &serial_com1;

	port->base = PORT_SERIAL;
	if (serial_detect(PORT_SERIAL) != 0) {
		return -1;
	}

	port->flags = SERIAL_PRESENT;

	outb(port->base + 1, 0x00); // Disable all interrupts
	outb(port->base + 3, 0x80); // Enable DLAB (set bound rate divisor)
	outb(port->base + 0, 0x03); // Set divisor to 3 (38400 bound)
	outb(port->base + 1, 0x00);  /* High byte of divisor */
    outb(port->base + 3, 0x03);  /* 8 bits, no parity, one stop bit */
    outb(port->base + 2, 0xC7);  /* Enable FIFO, clear them, 14-byte threshold */
    outb(port->base + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */

	port->flags |= SERIAL_FIF0;
	return 0;
}

kscope_node_t serial_node = {
	.name = "serial-uart",
	.id = 0x000B,
	.class = KSCOPE_CLASS_DRIVER,
	.subclass = KSCOPE_SUBCLASS_DRIVER_SERIAL,
	.requires = NULL,
	.require_count = 0,
	.provides = (const char *[]){"io.serial", "debug.port"},
	.provide_count = 2,
	.init = serial_init
};

// static int serial_transmit_empty(void) {
// 	return inb(PORT_SERIAL + 5) & 0x20;
// }

void serial_putc(serial_port_t *port, char c) {
    if (!(port->flags & SERIAL_PRESENT))
        return;

    while (!(inb(port->base + 5) & 0x20));
    outb(port->base, c);
}

void serial_puts(serial_port_t *port, const char *s) {
    if (!s)
        return;
    while (*s)
        serial_putc(port, *s++);
}

void serial_putc_default(char c) {
    serial_putc(&serial_com1, c);
}

void serial_puts_default(const char *s) {
    serial_puts(&serial_com1, s);
}

int serial_getc(serial_port_t *port) {
	if (!(port->flags & SERIAL_PRESENT))
        return -1;

    if (!(inb(port->base + 5) & 0x01))
        return -1;  /* No data */

    return inb(port->base);
}

int serial_getc_default(void) {
	return serial_getc(&serial_com1);
}