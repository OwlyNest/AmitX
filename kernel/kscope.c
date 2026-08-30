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
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

/* FUTURE: TOPOLOGICAL SORT!!!!!!*/

/* --- Macros ---*/
/* --- Includes ---*/
#include <fs/amfs.h>
#include <internal/kscope.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <screen/printk.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static kscope_node_t *registry[KSCOPE_MAX_NODES];
static size_t registry_count = 0;

/* --- Prototypes ---*/
static int kscope_check_deps(const kscope_node_t *node);
static int kscope_topo_sort(kscope_node_t **out_order, size_t *out_count);
static void kscope_mark_failed(kscope_node_t *node, const char *reason);

/* --- Functions ---*/

/* ==========================================================================
 * Register a node in the KScope registry
 * ======================================================================= */
void kscope_register(kscope_node_t *node) {
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
      printk("[kscope] Error: duplicate ID 0x%04x (%s and %s)\n", node->id,
             registry[i]->name, node->name);
      return;
    }
  }

  node->state = KSCOPE_STATE_REGISTERED;
  registry[registry_count++] = node;
  printk("[kscope] Registered: %s (0x%04x, class %d)\n", node->name, node->id,
         node->class);
}

/* ==========================================================================
 * Check if all dependencies of a node are in OK state
 * ======================================================================= */
static int kscope_check_deps(const kscope_node_t *node) {
  for (size_t i = 0; i < node->require_count; i++) {
    kscope_node_t *req = node->requires[i];
    if (!req) {
      printk("[kscope] %s: null dependency at index %u\n", node->name,
             (unsigned)i);
      return 0;
    }
    if (req->state != KSCOPE_STATE_OK) {
      printk("[kscope] %s requires %s (state: %s)\n", node->name, req->name,
             req->state == KSCOPE_STATE_FAILED    ? "FAILED"
             : req->state == KSCOPE_STATE_PROBING ? "PROBING"
                                                  : "not ready");
      return 0;
    }
  }
  return 1;
}

/* ==========================================================================
 * Mark a node and all its un-initialized dependents as FAILED
 * ======================================================================= */
static void kscope_mark_failed(kscope_node_t *node, const char *reason) {
  node->state = KSCOPE_STATE_FAILED;
  printk("[kscope] %s: FAILED (%s)\n", node->name, reason);

  /* Propagate failure to dependents that haven't run yet */
  for (size_t i = 0; i < registry_count; i++) {
    kscope_node_t *dep = registry[i];
    if (dep->state != KSCOPE_STATE_REGISTERED)
      continue;

    for (size_t j = 0; j < dep->require_count; j++) {
      if (dep->requires[j] == node) {
        kscope_mark_failed(dep, "dependency failed");
        break;
      }
    }
  }
}

/* ==========================================================================
 * Topological sort with cycle detection (DFS-based)
 *
 * Returns 0 on success, -1 if a cycle was detected.
 * On cycle, the nodes involved are marked FAILED.
 * ======================================================================= */
static int kscope_topo_sort(kscope_node_t **out_order, size_t *out_count) {
  uint8_t visited[KSCOPE_MAX_NODES] = {0};
  /* 0 = unvisited, 1 = visiting, 2 = done */

  *out_count = 0;

  for (size_t i = 0; i < registry_count; i++) {
    if (visited[i] == 2) {
      continue;
    }

    /* DFS stack: node index */
    size_t stack[KSCOPE_MAX_NODES];
    size_t stack_ptr = 0;
    stack[stack_ptr++] = i;

    while (stack_ptr > 0) {
      size_t idx = stack[stack_ptr - 1];

      if (visited[idx] == 2) {
        stack_ptr--;
        continue;
      }

      if (visited[idx] == 1) {
        /* Cycle detected: mark all nodes in current path */
        printk("[kscope] Cycle detected involving:\n");
        for (size_t j = 0; j < stack_ptr; j++) {
          printk("  %s\n", registry[stack[j]]->name);
          registry[stack[j]]->state = KSCOPE_STATE_FAILED;
        }
        return -1;
      }

      visited[idx] = 1;

      /* Push unvisited dependencies first */
      kscope_node_t *node = registry[idx];
      if (node->requires != NULL && ((uintptr_t)node->requires < 0xC0000000u ||
                                     (uintptr_t)node->requires > 0xC1000000u)) {
        __asm__ __volatile__("mov %0, %%eax\n"
                             "mov %1, %%ebx\n"
                             "mov $0xBAD001, %%ecx\n"
                             "hlt"
                             :
                             : "r"(node->requires), "r"(node)
                             : "eax", "ebx", "ecx");
      }
      int has_unvisited_deps = 0;

      for (size_t d = 0; d < node->require_count; d++) {
        kscope_node_t *req = node->requires[d];
        if (!req)
          continue;

        /* Find req in registry */
        size_t req_idx = registry_count;
        for (size_t k = 0; k < registry_count; k++) {
          if (registry[k] == req) {
            req_idx = k;
            break;
          }
        }

        if (req_idx >= registry_count) {
          printk("[kscope] %s depends on unregistered %s\n", node->name,
                 req->name);
          kscope_mark_failed(node, "unregistered dependency");
          has_unvisited_deps = 0; /* abort this branch */
          break;
        }

        if (visited[req_idx] == 0) {
          stack[stack_ptr++] = req_idx;
          has_unvisited_deps = 1;
        }
      }

      if (!has_unvisited_deps) {
        /* All deps resolved, mark done and add to output */
        visited[idx] = 2;
        stack_ptr--;
        out_order[(*out_count)++] = node;
      }
    }
  }

  return 0;
}

/* ==========================================================================
 * Probe all registered nodes in dependency order
 * ======================================================================= */
void kscope_probe_all(void) {
  kscope_node_t *order[KSCOPE_MAX_NODES];
  size_t order_count = 0;

  printk("[kscope] Probing %d nodes...\n", (int)registry_count);

  if (kscope_topo_sort(order, &order_count) != 0) {
    printk("[kscope] Aborting: dependency cycle detected\n");
    return;
  }

  for (size_t i = 0; i < order_count; i++) {
    kscope_node_t *node = order[i];

    if (node->state != KSCOPE_STATE_REGISTERED) {
      /* Already marked failed by dependency propagation */
      continue;
    }

    if (!kscope_check_deps(node)) {
      kscope_mark_failed(node, "unsatisfied dependencies");
      continue;
    }

    if (!node->init) {
      node->state = KSCOPE_STATE_OK;
      printk("[kscope] OK: %s (no init)\n", node->name);
      continue;
    }

    printk("[kscope] Init: %s...\n", node->name);
    node->state = KSCOPE_STATE_PROBING;

    int err = node->init();
    if (err != 0) {
      kscope_mark_failed(node, "init returned error");
      continue;
    }

    node->state = KSCOPE_STATE_OK;
    printk("[kscope] OK: %s\n", node->name);
  }
}

/* ==========================================================================
 * Shutdown all initialized nodes in reverse dependency order
 * ======================================================================= */
void kscope_shutdown_all(void) {
  /* Build reverse topo order: dependents first */
  kscope_node_t *order[KSCOPE_MAX_NODES];
  size_t order_count = 0;

  if (kscope_topo_sort(order, &order_count) != 0) {
    return;
  }

  for (size_t i = order_count; i > 0; i--) {
    kscope_node_t *node = order[i - 1];

    if (node->state != KSCOPE_STATE_OK) {
      continue;
    }

    if (node->shutdown) {
      printk("[kscope] Shutdown: %s...\n", node->name);
      node->shutdown();
    }

    node->state = KSCOPE_STATE_REGISTERED;
  }
}

/* ==========================================================================
 * Finders
 * ======================================================================= */
const kscope_node_t *kscope_find_by_name(const char *name) {
  for (size_t i = 0; i < registry_count; i++) {
    if (strcmp(registry[i]->name, name) == 0)
      return registry[i];
  }
  return NULL;
}

const kscope_node_t *kscope_find_by_id(uint32_t id) {
  for (size_t i = 0; i < registry_count; i++) {
    if (registry[i]->id == id)
      return registry[i];
  }
  return NULL;
}

/* ==========================================================================
 * Name helpers
 * ======================================================================= */
const char *kscope_class_name(kscope_class_t class) {
  switch (class) {
  case KSCOPE_CLASS_CORE:
    return "Core";
  case KSCOPE_CLASS_MEMORY:
    return "Memory";
  case KSCOPE_CLASS_INTERRUPT:
    return "Interrupt";
  case KSCOPE_CLASS_TIME:
    return "Time";
  case KSCOPE_CLASS_DRIVER:
    return "Driver";
  case KSCOPE_CLASS_STORAGE:
    return "Storage";
  case KSCOPE_CLASS_FS:
    return "FileSystem";
  case KSCOPE_CLASS_NETWORK:
    return "Network";
  case KSCOPE_CLASS_PROCESS:
    return "Process";
  case KSCOPE_CLASS_UI:
    return "UserInterface";
  case KSCOPE_CLASS_POWER:
    return "Power";
  case KSCOPE_CLASS_GFX:
    return "GFX";
  default:
    return "Unknown";
  }
}

const char *kscope_subclass_name(uint32_t subclass) {
  uint8_t cls = KSCOPE_SUBCLASS_CLASS(subclass);

  switch (subclass) {
  /* Core (0x00xx) */
  case KSCOPE_SUBCLASS_CORE_GDT:
    return "GDT";
  case KSCOPE_SUBCLASS_CORE_IDT:
    return "IDT";
  case KSCOPE_SUBCLASS_CORE_TSS:
    return "TSS";
  case KSCOPE_SUBCLASS_CORE_PIC:
    return "PIC";
  case KSCOPE_SUBCLASS_CORE_PCI:
    return "PCI";
  case KSCOPE_SUBCLASS_CORE_CPUID:
    return "CPUID";

  /* Memory (0x01xx) */
  case KSCOPE_SUBCLASS_MEMORY_PMM:
    return "PMM";
  case KSCOPE_SUBCLASS_MEMORY_VMM:
    return "VMM";
  case KSCOPE_SUBCLASS_MEMORY_HEAP:
    return "Heap";
  case KSCOPE_SUBCLASS_MEMORY_PAGING:
    return "Paging";

  /* Time (0x03xx) */
  case KSCOPE_SUBCLASS_TIME_PIT:
    return "PIT";
  case KSCOPE_SUBCLASS_TIME_SCHED:
    return "SCHEDULER";

  /* Driver (0x04xx) */
  case KSCOPE_SUBCLASS_DRIVER_KEYBOARD:
    return "Keyboard";
  case KSCOPE_SUBCLASS_DRIVER_MOUSE:
    return "Mouse";
  case KSCOPE_SUBCLASS_DRIVER_SERIAL:
    return "Serial";

  /* Storage (0x05xx) */
  case KSCOPE_SUBCLASS_STORAGE_CONTROLLER:
    return "Controller";
  case KSCOPE_SUBCLASS_STORAGE_IDE:
    return "IDE";

  /* FS (0x06xx) */
  case KSCOPE_SUBCLASS_FS_VFS:
    return "VFS";
  case KSCOPE_SUBCLASS_FS_AMFS:
    return "AMFS";
  case KSCOPE_SUBCLASS_FS_RAMFS:
    return "RAMFS";

  /* Network (0x07xx) */
  case KSCOPE_SUBCLASS_NETWORK_E1000:
    return "E1000";

  /* UI (0x09xx) */
  case KSCOPE_SUBCLASS_UI_SCREEN:
    return "Screen";

  /* Power (0x0Axx) */
  case KSCOPE_SUBCLASS_POWER_ACPI:
    return "ACPI";

  /* GFX (0x0Bxx) */
  case KSCOPE_SUBCLASS_GFX_SVGA:
    return "SVGA";

  default: {
    static char buf[32];
    ksnprintf(buf, sizeof(buf), "Unknown(0x%02x/0x%04x)", cls, subclass);
    return buf;
  }
  }
}

/* ==========================================================================
 * Debug output
 * ======================================================================= */
void kscope_dump_node(const kscope_node_t *node) {
  const char *state_str;

  switch (node->state) {
  case KSCOPE_STATE_REGISTERED:
    state_str = "Registered";
    break;
  case KSCOPE_STATE_PROBING:
    state_str = "Probing";
    break;
  case KSCOPE_STATE_OK:
    state_str = "OK";
    break;
  case KSCOPE_STATE_FAILED:
    state_str = "FAILED";
    break;
  default:
    state_str = "Unknown";
    break;
  }

  printk("KScope %s %s, %s (0x%04x)\n", kscope_class_name(node->class),
         kscope_subclass_name(node->subclass), node->name, node->id);

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

  printk("State: %s\n", state_str);
}

void kscope_dump(void) {
  printk("\n=== KScope Subsystem List ===\n");
  for (size_t i = 0; i < registry_count; i++) {
    printk("=============\n");
    kscope_dump_node(registry[i]);
  }
  printk("=======================\n\n");
}

/* ==========================================================================
 * Log to filesystem
 * ======================================================================= */
void kscope_log_to_fs(void) {
  int bufsize = 8192;
  char *buf = (char *)malloc(bufsize);
  if (!buf) {
    printk("[kscope] Failed to allocate buffer\n");
    return;
  }

  int pos = 0;
  int truncated = 0;

  pos += ksnprintf(buf + pos, bufsize - pos,
                   "=== KScope Subsystem Inventory ===\n\n");

  for (size_t i = 0; i < registry_count; i++) {
    const kscope_node_t *node = registry[i];

    if (pos >= bufsize - 256) {
      truncated = 1;
      break;
    }

    pos += ksnprintf(buf + pos, bufsize - pos,
                     "[%u] %s (ID: 0x%04x, Class: %s/%s)\n", (unsigned)i,
                     node->name, node->id, kscope_class_name(node->class),
                     kscope_subclass_name(node->subclass));

    pos += ksnprintf(buf + pos, bufsize - pos, "    State: %s\n",
                     node->state == KSCOPE_STATE_OK        ? "OK"
                     : node->state == KSCOPE_STATE_FAILED  ? "FAILED"
                     : node->state == KSCOPE_STATE_PROBING ? "PROBING"
                                                           : "REGISTERED");

    if (node->require_count > 0) {
      pos += ksnprintf(buf + pos, bufsize - pos, "    Requires: ");
      for (size_t j = 0; j < node->require_count; j++) {
        pos +=
            ksnprintf(buf + pos, bufsize - pos, " %s", node->requires[j]->name);
      }
      pos += ksnprintf(buf + pos, bufsize - pos, "\n");
    }

    if (node->provide_count > 0) {
      pos += ksnprintf(buf + pos, bufsize - pos, "    Provides:\n");
      for (size_t j = 0; j < node->provide_count; j++) {
        pos += ksnprintf(buf + pos, bufsize - pos, "        %s\n",
                         node->provides[j]);
      }
    }

    pos += ksnprintf(buf + pos, bufsize - pos, "\n");
  }

  if (truncated)
    pos += ksnprintf(buf + pos, bufsize - pos, "\n[... truncated ...]\n");

  pos += ksnprintf(buf + pos, bufsize - pos, "=== End of Inventory ===\n");

  amfs_write("/kscope_inventory.txt", buf, pos);
  free(buf);
}

/* ==========================================================================
 * Registry accessors
 * ======================================================================= */
size_t kscope_get_count(void) { return registry_count; }

const kscope_node_t *kscope_get_node(size_t idx) {
  if (idx >= registry_count)
    return NULL;
  return registry[idx];
}
