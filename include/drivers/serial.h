/*
	* drivers/serial.h - [Enter description]
	* Author:   amity
	* Date:     Wed Jun 10 15:48:03 2026
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
#ifndef SERIAL_H
#define SERIAL_H
#define SERIAL_RX_BUFSZ 256
/* --- Includes ---*/

/* --- Typedefs - Structs - Enums ---*/
#include <stdint.h>
typedef struct {
	uint16_t base;
	uint16_t baud_div;
	uint8_t  flags;
#define SERIAL_PRESENT 0x01
#define SERIAL_FIF0    0x02
#define SERIAL_IRQ_EN  0x04
	volatile uint8_t rx_buf[SERIAL_RX_BUFSZ];
	volatile uint16_t rx_head;
	volatile uint16_t rx_tail;
} serial_port_t;

/* --- Globals ---*/
extern serial_port_t serial_com1;
/* --- Prototypes ---*/
void serial_putc(serial_port_t *port, char c);
void serial_puts(serial_port_t *port, const char *s);
void serial_putc_default(char c);
void serial_puts_default(const char *c);

int serial_getc(serial_port_t *port);  /* Returns char, or -1 if none */
int serial_getc_default(void);
int serial_gets(serial_port_t *port, char *buf, int buflen);
int serial_gets_default(char *buf, int buflen);
#endif