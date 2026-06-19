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

/*
    * todo
    * 1) Multiple COM ports — probe COM1-COM4, register per-port KScope nodes
    * 2) Baud rate selection — runtime config, not hardcoded 38400
    * 3) Line status/error handling — overrun, parity, framing errors
    * 4) Transmit IRQ — interrupt-driven TX for faster output
    * 5) Flow control — RTS/CTS for reliable high-speed comms
    * 6) Locking — spinlock for SMP safety (future)
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include "arch/x86/interrupts.h"
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

static void serial_irq_handler(interrupt_frame_t *frame) {
	(void)frame;

	serial_port_t *port = &serial_com1;

    while (inb(port->base + 5) & 0x01) {  /* LSR bit 0 = Data Ready */
        uint8_t c = inb(port->base);
        uint16_t next = (port->rx_head + 1) % SERIAL_RX_BUFSZ;
        if (next != port->rx_tail) {  /* drop if full */
            port->rx_buf[port->rx_head] = c;
            port->rx_head = next;
        }
    }
}

static int serial_init(void) {
    serial_port_t *port = &serial_com1;

    port->base = PORT_SERIAL;
    port->rx_head = 0;
    port->rx_tail = 0;

    if (serial_detect(port->base) != 0)
        return -1;

    port->flags = SERIAL_PRESENT;

    outb(port->base + 1, 0x00);  /* Disable interrupts during setup */
    outb(port->base + 3, 0x80);  /* DLAB */
    outb(port->base + 0, 0x03);  /* Divisor low */
    outb(port->base + 1, 0x00);  /* Divisor high */
    outb(port->base + 3, 0x03);  /* 8N1 */
    outb(port->base + 2, 0xC7);  /* FIFO enable, clear, 14-byte thresh */
    outb(port->base + 4, 0x0B);  /* DTR, RTS, OUT2 */

    port->flags |= SERIAL_FIF0;

    /* Enable Received Data Available interrupt */
    outb(port->base + 1, 0x01);  /* IER bit 0 = RDAI */
    port->flags |= SERIAL_IRQ_EN;

	register_interrupt_handler(VECTOR_IRQ4, serial_irq_handler);
	pic_unmask_irq(4);

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

    if (port->flags & SERIAL_IRQ_EN) {
        /* Read from buffer */
        if (port->rx_head == port->rx_tail)
            return -1;
        uint8_t c = port->rx_buf[port->rx_tail];
        port->rx_tail = (port->rx_tail + 1) % SERIAL_RX_BUFSZ;
        return c;
    } else {
        /* Polling fallback */
        if (!(inb(port->base + 5) & 0x01))
            return -1;
        return inb(port->base);
    }
}

int serial_getc_default(void) {
	return serial_getc(&serial_com1);
}

int serial_gets(serial_port_t *port, char *buf, int buflen) {
    for (int i = 0; i < buflen - 1; i++) {
        int c = serial_getc(port);
        if (c == -1) {
            /* No data available — return what we have, or block? */
            buf[i] = '\0';
            return i;  /* return chars read so far */
        }
        if (c == '\n' || c == '\r') {
            buf[i] = '\0';
            return i;
        }
        buf[i] = (char)c;
    }
    buf[buflen - 1] = '\0';
    return buflen - 1;
}

int serial_gets_default(char *buf, int buflen) {
    return serial_gets(&serial_com1, buf, buflen);
}