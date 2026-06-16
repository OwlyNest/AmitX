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

/* FUTURE: TOPOLOGICAL SORT!!!!!!*/

/* --- Macros ---*/
/* --- Includes ---*/
#include <internal/kscope.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <fs/amfs.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static kscope_node_t *registry[KSCOPE_MAX_NODES];
static size_t registry_count;
/* --- Prototypes ---*/

/* --- Functions ---*/

void kscope_register(kscope_node_t* node) {
	if (!node || !node->name) {
		printk("[kscope] Error: register called with null node\n");
		return;
	}
	if (registry_count >= KSCOPE_MAX_NODES) {
		printk("[kscope] Error: registry full\n");
		return;
	}

	for (size_t i = 0; i < registry_count; i++) {
		if (registry[i]->id == node->id) {
			printk("[kscope] Error: duplicate ID 0x%04x (%s and %s)\n", node->id, registry[i]->name, node->name);
			return;
		}
	}
	node->state = 0;
	registry[registry_count++] = node;
	printk("[kscope] Registered: %s (0x%04x, class %d)\n", node->name, node->id, node->class);
	
}

void kscope_probe_all(void) {
    printk("[kscope] Probing %d nodes...\n", (int)registry_count);

    for (size_t i = 0; i < registry_count; i++) {
        kscope_node_t *node = registry[i];

        /* Validate requirements are satisfied (probed before us) */
        for (size_t j = 0; j < node->require_count; j++) {
            kscope_node_t *req = node->requires[j];
            int satisfied = 0;
            
            for (size_t k = 0; k < i; k++) {
                if (registry[k] == req) {
                    satisfied = 1;
                    break;
                }
            }
            
            if (!satisfied) {
                printk("[kscope] ERROR: %s requires %s (not probed yet)\n",
                       node->name, req->name);
                /* Skip this node, don't init */
                goto next_node;
            }
        }

        if (node->init) {
            printk("[kscope] Init: %s...\n", node->name);
            int err = node->init();
			node->state = 1;
            if (err != 0) {
                printk("[kscope] FAILED: %s (err: %d)\n", node->name, err);
                /* TODO: mark failed, block dependents */
                continue;
            }
            printk("[kscope] OK: %s\n", node->name);
        }
    next_node:;
    }
}

kscope_node_t *kscope_find_by_name(const char *name) {
	for (size_t i = 0; i < registry_count; i++) {
		if (strcmp(registry[i]->name, name) == 0) {
			return registry[i];
		}
	}
	return NULL;
}
kscope_node_t *kscope_find_by_id(uint32_t id) {
	for (size_t i = 0; i < registry_count; i++) {
		if (registry[i]->id == id) {
			return registry[i];
		}
	}
	return NULL;
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

const char *kscope_subclass_name(uint32_t subclass) {
	switch (subclass) {
		/* Core (0x00xx)*/
		case KSCOPE_SUBCLASS_CORE_GDT: return "GDT";
		case KSCOPE_SUBCLASS_CORE_IDT: return "IDT";
		case KSCOPE_SUBCLASS_CORE_TSS: return "TSS";
		case KSCOPE_SUBCLASS_CORE_PIC: return "PIC";
		case KSCOPE_SUBCLASS_CORE_PCI: return "PCI";

		/* Memory (0x01xx) */
		case KSCOPE_SUBCLASS_MEMORY_PMM: return "PMM";
		case KSCOPE_SUBCLASS_MEMORY_HEAP: return "Heap";
		/* Interrupt (0x02xx) */
		/* Time (0x03xx) */
		case KSCOPE_SUBCLASS_TIME_PIT: return "PIT";
		/* Driver (0x04xx) */
		case KSCOPE_SUBCLASS_DRIVER_KEYBOARD: return "Keyboard";
		case KSCOPE_SUBCLASS_DRIVER_MOUSE: return "Mouse";
		case KSCOPE_SUBCLASS_DRIVER_SERIAL: return "Serial";
		/* Storage (0x05xx) */
		case KSCOPE_SUBCLASS_STORAGE_CONTROLLER: return "Controller";
		case KSCOPE_SUBCLASS_STORAGE_IDE: return "IDE";
		/* FS (0x06xx) */
		case KSCOPE_SUBCLASS_FS_VFS: return "VFS";
		case KSCOPE_SUBCLASS_FS_AMFS: return "AMFS";
		case KSCOPE_SUBCLASS_FS_RAMFS: return "RAMFS";
		/* Network (0x07xx) */
		case KSCOPE_SUBCLASS_NETWORK_E1000: return "E1000";
		/* Process (0x08xx) */
		/* UI (0x09xx) */
		case KSCOPE_SUBCLASS_UI_SCREEN: return "Screen";
		/* Power (0x0Axx) */
		case KSCOPE_SUBCLASS_POWER_ACPI: return "ACPI";
		default: return "Unknown";
	}
}

void kscope_dump_node(kscope_node_t *node) {
	printk("KScope %s %s, %s (%d)\n",
		kscope_class_name(node->class),
		kscope_subclass_name(node->subclass),
		node->name,
		node->id
	);

	if (node->require_count != 0) {
		printk("Requires: ");
		for (size_t i = 0; i < node->require_count; i++) {
			printk("%s ", node->requires[i]->name);
		}
		printk("\n");
	}

	if (node->provide_count != 0) {
		printk("Provides:\n");
		for (size_t i = 0; i < node->provide_count; i++) {
			printk("\t%s\n", node->provides[i]);
		}
	}
	printk("State %s\n", node->state == 1 ? "OK" : "Registered");
}

void kscope_dump(void) {
	printk("\n=== KScope Subsystem List ===\n");
	for (size_t i = 0; i < registry_count; i++) {
		printk("=============\n");
		kscope_dump_node(registry[i]);
	}
	printk("=======================\n\n");
}

void kscope_log_to_fs(void) {
	int bufsize = 2*4096;
	char *buf = (char *)malloc(bufsize);
	if (!buf) {
		printk("[kscope] Failed to allocate buffer\n");
		return;
	}

	int pos = 0;
	pos += ksnprintf(buf + pos, bufsize - pos, "=== KScope Subsystem Inventory ===\n\n");

	for (size_t i = 0; i < registry_count; i++) {
		kscope_node_t *node = registry[i];

		pos += ksnprintf(buf + pos, bufsize - pos, "[%u] %s (ID: 0x%04x, Class: %s/%s)\n", i, node->name, node->id, kscope_class_name(node->class), kscope_subclass_name(node->subclass));

		pos += ksnprintf(buf + pos, bufsize - pos, "    State: %s\n",node->state == 1 ? "OK" : (node->state == -1 ? "FAILED" : "REGISTERED"));

        if (node->require_count > 0) {
            pos += ksnprintf(buf + pos, bufsize - pos, "    Requires: ");
            for (size_t j = 0; j < node->require_count; j++) {
                pos += ksnprintf(buf + pos, bufsize - pos, " %s",node->requires[j]->name);
            }
            pos += ksnprintf(buf + pos, bufsize - pos, "\n");
        }

		if (node->provide_count > 0) {
            pos += ksnprintf(buf + pos, bufsize - pos, "    Provides:\n");
            for (size_t j = 0; j < node->provide_count; j++) {
                pos += ksnprintf(buf + pos, bufsize - pos, "        %s\n", node->provides[j]);
            }
        }

        pos += ksnprintf(buf + pos, bufsize - pos, "\n");
	}

	pos += ksnprintf(buf + pos, bufsize - pos, "=== End of Inventory ===\n");

    amfs_write_file("kscope_inventory.txt", buf, pos);
    free(buf);
}

size_t kscope_get_count(void) {
    return registry_count;
}

kscope_node_t *kscope_get_node(size_t idx) {
    if (idx >= registry_count) return NULL;
    return registry[idx];
}