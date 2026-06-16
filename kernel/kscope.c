/*
	* kernel/kscope.c - [Enter description]
	* Author:   amity
	* Date:     Tue Jun 16 08:57:44 2026
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
#define KSCOPE_MAX_NODES = 32
/* --- Includes ---*/
#include <kscope.h>
#include <screen/printk.h>
#include <lib/string.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static kscope_node_t *registry[KSCOPE_MAX_NODES];
static size_t registry_count;
/* --- Prototypes ---*/

/* --- Functions ---*/

void kscope_register(kscope_node_t* node) {
	if (!node || !node->name) {
		printk("[kscope] error: register called with null node\n");
		return;
	}
	if (registry_count >= KSCOPE_MAX_NODES) {
		printk("[kscope] error: registry full\n");
		return;
	}

	for (size_t i = 0; i < registry_count; i++) {
		if (registry[i]->id == node->id) {
			printk("[kscope] error: duplicate ID 0x%04x (%s and %s)\n", node->id, registry[i]->name, node->name);
			return;
		}
	}
	registry[registry_count++] = node;
	printk("[kscope] Registered: %s (0x%04x, class %d)\n", node->name, node->id, node->class);
}

void kscope_probe_all(void) {
	/* TODO: dependancy resolution */
	printk("[kscope] Probing %d nodes...\n", (int)registry_count);

	for (size_t i = 0; i < registry_count; i++) {
		kscope_node_t *node = registry[i];

		/* TODO: check requirements */

		if (node->init) {
			printk("[kscope] Init: %s...\n", node->name);
			int err = node->init();
			if (err != 0) {
				printk("[kscope] Failed: %s (err: %d)\n", node->name, err);
				/* TODO: mark as failed, block dependant */
				continue;
			}

			printk("[kscope] OK: %s\n", node->name);
		}
	}
	return;
}

kscope_node_t *kscope_find_by_name(const char *name) {
	for (size_t i = 0; i < registry_count; i++) {
		if (strcmp(registry[i]->name, name) == 0) {
			return registry[i];
		}
	}
}
kscope_node_t *kscope_find_by_id(uint32_t id) {
	for (size_t i = 0; i < registry_count; i++) {
		if (registry[i]->id == id) {
			return registry[i];
		}
	}
}

const char *kscope_class_name(kscope_class_t class) {
	switch (class) {
		case KSCOPE_CLASS_CORE:      return "Core";
		case KSCOPE_CLASS_MEMORY:    return "Memory";
		case KSCOPE_CLASS_INTERRUPT: return "Interrupt";
		case KSCOPE_CLASS_TIME:      return "Time";
		case KSCOPE_CLASS_DRIVER:    return "Driver";
		case KSCOPE_CLASS_STORAGE:   return "Storage";
		case KSCOPE_CLASS_FS:        return "FileSystem";
		case KSCOPE_CLASS_NETWORK:   return "Network";
		case KSCOPE_CLASS_PROCESS:   return "Process";
		case KSCOPE_CLASS_UI:        return "UserInterface";
		case KSCOPE_CLASS_POWER:     return "Power";
		default:                     return "Unknown";
	}
}

const char *kscope_subclass_name(kscope_class_t class, uint32_t subclass) {
	switch (class) {
		case KSCOPE_CLASS_CORE: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_MEMORY: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_INTERRUPT: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_TIME: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_DRIVER: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_STORAGE: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_FS: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_NETWORK: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_PROCESS: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_UI: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		case KSCOPE_CLASS_POWER: {
			switch (subclass) {
				default: return "Unknown";
			}
		}
		default:                     return "Unknown";
	}
}

void kscope_dump_node(kscope_node_t *node) {
	printk("KScope %s %s\n",
		kscope_subclass_name(node->class, node->subclass),
		kscope_class_name(node->class)
	);

	if (node->require_count != 0) {
		printk("Requires: ");
		for (size_t i = 0; i < node->require_count; i++) {
			printk("%s ", node->requires[i]->name);
		}
		printk("\n");
	}

	if (node->provide_count != 0) {
		printk("Provides: ");
		for (size_t i = 0; i < node->provide_count; i++) {
			printk("%s ", node->provides[i]);
		}
		printk("\n");
	}
}

void kscope_dump(void) {
	printk("\n=== KScope Subsystem List ===\n");
	for (size_t i = 0; i < registry_count; i++) {
		kscope_dump_node(registry[i]);
	}
	printk("=======================\n\n");
}