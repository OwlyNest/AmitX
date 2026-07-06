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
#ifndef __INTERNAL_KSCOPE_H__
#define __INTERNAL_KSCOPE_H__

#define KSCOPE_MAX_NODES 32
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
    KSCOPE_CLASS_GFX       = 0x0B,
} kscope_class_t;

typedef enum {
    /* Core (0x00xx) */
    KSCOPE_SUBCLASS_CORE_GDT = 0x0000,
    KSCOPE_SUBCLASS_CORE_IDT = 0x0001,
    KSCOPE_SUBCLASS_CORE_TSS = 0x0002,
    KSCOPE_SUBCLASS_CORE_PIC = 0x0003,
    KSCOPE_SUBCLASS_CORE_PCI = 0x0004,

    /* Memory (0x01xx) */
    KSCOPE_SUBCLASS_MEMORY_PMM  = 0x0100,
    KSCOPE_SUBCLASS_MEMORY_HEAP = 0x0101,

    /* Time (0x03xx) */
    KSCOPE_SUBCLASS_TIME_PIT   = 0x0300,
    KSCOPE_SUBCLASS_TIME_SCHED = 0x0301,

    /* Driver (0x04xx) */
    KSCOPE_SUBCLASS_DRIVER_KEYBOARD = 0x0400,
    KSCOPE_SUBCLASS_DRIVER_MOUSE    = 0x0401,
    KSCOPE_SUBCLASS_DRIVER_SERIAL   = 0x0402,

    /* Storage (0x05xx) */
    KSCOPE_SUBCLASS_STORAGE_CONTROLLER = 0x0500,
    KSCOPE_SUBCLASS_STORAGE_IDE        = 0x0501,

    /* FS (0x06xx) */
    KSCOPE_SUBCLASS_FS_VFS   = 0x0600,
    KSCOPE_SUBCLASS_FS_AMFS  = 0x0601,
    KSCOPE_SUBCLASS_FS_RAMFS = 0x0602,

    /* Network (0x07xx) */
    KSCOPE_SUBCLASS_NETWORK_E1000 = 0x0700,

    /* UI (0x09xx) */
    KSCOPE_SUBCLASS_UI_SCREEN = 0x0900,

    /* Power (0x0Axx) */
    KSCOPE_SUBCLASS_POWER_ACPI = 0x0A00,

    /* GFX (0x0Bxx) */
    KSCOPE_SUBCLASS_GFX_SVGA = 0x0B00,
} kscope_subclass_t;

#define KSCOPE_SUBCLASS_CLASS(sub) ((sub) >> 8)

typedef enum {
    KSCOPE_STATE_REGISTERED = 0,
    KSCOPE_STATE_PROBING    = 1,
    KSCOPE_STATE_OK         = 2,
    KSCOPE_STATE_FAILED     = -1,
} kscope_state_t;

struct kscope_node {
    const char *name;
    uint32_t id;
    kscope_class_t class;
    uint32_t subclass;

    kscope_node_t **requires;
    size_t require_count;

    const char **provides;
    size_t provide_count;

    int (*init)(void);
    void (*shutdown)(void);
    void *private;

    kscope_state_t state;
};
/* --- Globals ---*/

/* --- Prototypes ---*/
void kscope_register(kscope_node_t* node);
void kscope_register_all(void);
void kscope_probe_all(void);
void kscope_shutdown_all(void);

const kscope_node_t *kscope_find_by_name(const char *name);
const kscope_node_t *kscope_find_by_id(uint32_t id);

const char *kscope_class_name(kscope_class_t class);
const char *kscope_subclass_name(uint32_t subclass);

void kscope_dump(void);
void kscope_log_to_fs(void);

size_t kscope_get_count(void);
const kscope_node_t *kscope_get_node(size_t idx);
#endif