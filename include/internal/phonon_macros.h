/*
	* include/internal/phonon_macros.h - [Enter description]
	* Author:   amity
	* Date:     Sun Jun 28 15:25:54 2026
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
#ifndef __INTERNAL_PHONON_MACROS_H__
#define __INTERNAL_PHONON_MACROS_H__

#define ASSERT(cond)                                  \
do {                                                  \
    if (!(cond)) {                                    \
        printk("[ASSERT] %s:%d: %s\n",                \
               __FILE__, __LINE__, #cond);            \
        for (;;)                                      \
            __asm__ volatile ("cli; hlt");            \
    }                                                 \
} while (0)

#define WARN_IF(cond) \
    do { if (cond) { \
        printk("[WARN] %s:%d: " #cond "\n", __FILE__, __LINE__); \
    } } while (0)

#define EXPORT_SYMBOL(sym) \
    static export_t __export_##sym __attribute__((used, section(".exports"))) = { \
        .name = #sym, \
        .func = (void *)sym \
    }

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
/* --- Includes ---*/

/* --- Typedefs - Structs - Enums ---*/
typedef struct {
    const char *name;
    void *func;
} export_t;
/* --- Globals ---*/

/* --- Prototypes ---*/
#endif