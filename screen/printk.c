/*
	* screen/printk.c - [Enter description]
	* Author:   amity
	* Date:     Thu Jun 11 10:01:53 2026
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
#include <screen/printk.h>
#include <screen/screen.h>
#include <drivers/serial.h>
#include <lib/string.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

static void emit_char(char **out, size_t *remaining, int *count, char c) {
    if (*remaining > 1) {
        **out = c;
        (*out)++;
        (*remaining)--;
		(*count)++;
    }
}

static void emit_string(char **out, size_t *remaining, int *count, const char *str) {
	while (*str) {
		emit_char(out, remaining, count, *str++);
	}
}

static void utoa_base(uintptr_t value, unsigned base, int uppercase, char *buf) {
	char tmp[65];
	int i = 0;

	const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

	if (base < 2 || base > 16) {
		buf[0] = '\0';
		return;
	}

	if (value == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return;
	}

	while (value > 0) {
		tmp[i++] = digits[value % base];
		value /= base;
	}

	int j = 0;

	while (i > 0) {
		buf[j++] = tmp[--i];
	}

	buf[j] = '\0';
}

static void format_unsigned(char **out, size_t *remaining, int *count, uintptr_t value, unsigned base, int uppercase, int width, char pad_char, int left_justify) {
    char tmp[65];
	utoa_base(value, base, uppercase, tmp);

	int len = strlen(tmp);
	int pad = (len < width) ? (width - len) : 0;

	if (!left_justify) {
		while (pad--) {
			emit_char(out, remaining, count, pad_char);
		}
	}

	emit_string(out, remaining, count, tmp);

	if (left_justify) {
		while (pad--) {
			emit_char(out, remaining, count, ' ');
		}
	}
}

static void format_signed(char **out, size_t *remaining, int *count, int value, unsigned base, int width, char pad_char) {
	char tmp[65];
	unsigned int abs;
	int len;
	int pad;

	if (value < 0) {
	emit_char(out, remaining, count, '-');
	abs = (unsigned int)(-value);

	/* '-' consumes one width slot */
	if (width > 0) {
	width--;
	}
	} else {
	abs = (unsigned int)value;
	}

	utoa_base(abs, base, 0, tmp);

	len = strlen(tmp);
	pad = (len < width) ? (width - len) : 0;

	while (pad--) {
	emit_char(out, remaining, count, pad_char);
	}

	emit_string(out, remaining, count, tmp);
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
	char *out = buf;
	size_t remaining = size;
	int count = 0;
	while (*fmt) {
		if (*fmt != '%') {
			emit_char(&out, &remaining, &count, *fmt);
			fmt++;
			continue;
		}
	
		fmt++;

		int left_justify = 0;
		char pad_char = ' ';
		int zero_pad = 0;
		int width = 0;

		while (*fmt == '-' || *fmt == '0') {
			if (*fmt == '-') {
				left_justify = 1;
			} else if (*fmt == '0') {
				zero_pad = 1;
			}
			fmt++;
		}
		
		if (left_justify) {
			pad_char = ' ';
		} else if (zero_pad) {
			pad_char = '0';
		}

		while (*fmt >= '0' && *fmt <= '9') {
			width = (width * 10) + (*fmt - '0');
			fmt++;
		}

		if (left_justify) {
			pad_char = ' ';
		}

		if (*fmt == '%') {
			emit_char(&out, &remaining, &count, '%');
			fmt++;
			continue;
		}
	
		switch (*fmt) {
			case 's': {
				const char *str = va_arg(args, const char *);

				if (!str) {
					str = "(null)";
				}

				emit_string(&out, &remaining, &count, str);

				break;
			}
			case 'd': {
				int value = va_arg(args, int);
				format_signed(&out, &remaining, &count, value, 10, width, pad_char);
				break;
			}
			case 'u': {
				unsigned int value = va_arg(args, unsigned int);
				format_unsigned(&out, &remaining, &count, value, 10, 0, width, pad_char, left_justify);
				break;
			}
			case 'x': {
				unsigned int value = va_arg(args, unsigned int);
				format_unsigned(&out, &remaining, &count, value, 16, 0, width, pad_char, left_justify);
				break;
			}
			case 'X': {
				unsigned int value = va_arg(args, unsigned int);
				format_unsigned(&out, &remaining, &count, value, 16, 1, width, pad_char, left_justify);
				break;
			}
			case 'c': {
				char c = (char)va_arg(args, int);
				emit_char(&out, &remaining, &count, c);
				break;
			}
			case 'p': {
				uintptr_t value = (uintptr_t)va_arg(args, void *);
			
				emit_string(&out, &remaining, &count, "0x");
			
				format_unsigned(&out, &remaining, &count, value, 16, 0, (int)(sizeof(uintptr_t) * 2), '0', 0);
				break;
			}
		}
	
		fmt++;
	}

	if (size > 0) {
		*out = '\0';
	} 
	return count;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int ret = kvsnprintf(buf, size, fmt, args);

    va_end(args);
    return ret;
}

void printk(const char *fmt, ...) {
    char buf[4096];

    va_list args;
    va_start(args, fmt);

    kvsnprintf(buf, sizeof(buf), fmt, args);

    va_end(args);

    puts(buf);
    serial_puts(buf);
}