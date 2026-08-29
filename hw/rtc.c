/*
	* hw/rtc.c - [Enter description]
	* Author:   amity
	* Date:     Sun Jun 28 17:17:55 2026
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
#include <hw/rtc.h>
#include <hw/acpi.h>
#include <screen/printk.h>
#include <arch/x86/io.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/
static uint8_t rtc_read_reg(uint8_t reg) {
	outb(RTC_INDEX, reg);
	return inb(RTC_DATA);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
	return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void rtc_read(rtc_time_t *t) {
	uint8_t status_b = rtc_read_reg(RTC_STATUS_B);
	int bcd = !(status_b & 0x04); /* Bit 2 = binary mode */
	acpi_fadt_t *fadt = acpi_get_fadt();

	t->second = rtc_read_reg(RTC_SECOND);
	t->minute = rtc_read_reg(RTC_MINUTE);
	t->hour   = rtc_read_reg(RTC_HOUR);
	t->day    = rtc_read_reg(RTC_DAY);
	t->month  = rtc_read_reg(RTC_MONTH);
	t->year   = rtc_read_reg(RTC_YEAR);

	if (bcd) {
		t->second = bcd_to_bin(t->second);
		t->minute = bcd_to_bin(t->minute);
		t->hour   = bcd_to_bin(t->hour);
		t->day    = bcd_to_bin(t->day);
		t->month  = bcd_to_bin(t->month);
		t->year   = bcd_to_bin(t->year);
	}
	if (fadt->century != 0) {
		uint8_t century = rtc_read_reg(fadt->century);

		if (bcd)
			century = bcd_to_bin(century);

		t->year += century * 100;
	} else {
		printk("[rtc] FADT has no century");
		t->year += 2000;
	}
}