/*
	* include/kscope.h - [Enter description]
	* Author:   amity
	* Date:     Tue Jun 16 08:57:53 2026
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
#ifndef KSCOPE_H
#define KSCOPE_H

/* --- Includes ---*/
#include <stdint.h>
#include <stddef.h>
/* --- Typedefs - Structs - Enums ---*/
typedef struct kscope_node kscope_node_t;
typedef enum {
	KSCOPE_CLASS_CORE      = 0x00,
	KSCOPE_CLASS_MEMORY    = 0x01,
	KSCOPE_CLASS_INTERRUPT = 0x02,
	KSCOPE_CLASS_TIME      = 0x03,
	KSCOPE_CLASS_DRIVER    = 0x04,
	KSCOPE_CLASS_STORAGE   = 0x05,
	KSCOPE_CLASS_FS        = 0x06,
	KSCOPE_CLASS_NETWORK   = 0x07,
	KSCOPE_CLASS_PROCESS   = 0x08,
	KSCOPE_CLASS_UI        = 0x09,
	KSCOPE_CLASS_POWER     = 0x0A,
} kscope_class_t;

struct kscope_node {
	const char *name;
	uint32_t id;
	kscope_class_t class;
	uint32_t subclass;

	kscope_node_t** requires;
	size_t require_count;

	const char **provides;
	size_t provide_count;

	int (*init)(void);
	void (*shutdown)(void);
	void *private;
};
/* --- Globals ---*/

/* --- Prototypes ---*/
void kscope_register(kscope_node_t* node);
void kscope_probe_all(void);

kscope_node_t *kscope_find_by_name(const char *name);
kscope_node_t *kscope_find_by_id(uint32_t id);

void kscope_dump(void);
#endif