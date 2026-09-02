/*
 * exec/loader2.c - AMX v2 executable loader
 * Author:   amity
 * Date:     Mon Jul 20 15:02:00 2026
 * Copyright (C) 2026 OwlyNest
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

/* --- Macros ---*/
#define EXEC2_STACK_SIZE 4096

/* --- Includes ---*/
#include <exec/amx2.h>
#include <exec/loader2.h>
#include <fs/amfs.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
static void *sect_ptr(exec2_context_t *ctx, uint32_t vaddr);
static int read_file(const char *path, uint8_t **out_buf, uint32_t *out_size);

/* --- Functions ---*/

/* ==========================================================================
 *                                                                          *
 * read_file()                                                              *
 *                                                                          *
 * Reads an entire file into a malloc'd buffer via VFS.                     *
 * Returns 0 on success, -1 on failure.                                     *
 *                                                                          *
 * TODO: Replace two-pass dance with vfs_stat() when available.             *
 *                                                                          *
 * ======================================================================== */
static int read_file(const char *path, uint8_t **out_buf, uint32_t *out_size) {
  if (!path || !out_buf || !out_size) {
    return -1;
  }

  if (path[0] != '/') {
    return -1;
  }

  if (strcmp(path, "/") == 0) {
    return -1;
  }

  if (!amfs_exists(path)) {
    return -1;
  }

  /* Pass 1: read header to determine total size */
  uint8_t hdr_buf[AMX2_HEADER_SIZE];
  int len = amfs_read(path, (char *)hdr_buf, sizeof(hdr_buf));
  if (len < AMX2_HEADER_SIZE) {
    return -1;
  }

  amx2_header_t header;
  if (amx2_read_header(hdr_buf, &header) != 0) {
    return -1;
  }

  if (amx2_verify_header(&header) != AMX2_OK) {
    return -1;
  }

  uint32_t pass1_size = AMX2_HEADER_SIZE +
                        header.section_count * AMX2_SECTION_SIZE +
                        header.string_size;

  uint8_t *pass1 = (uint8_t *)malloc(pass1_size);
  if (!pass1) {
    return -1;
  }

  len = amfs_read(path, (char *)pass1, pass1_size);
  if (len < (int)pass1_size) {
    free(pass1);
    return -1;
  }

  /* Parse sections to find max file offset */
  amx2_section_t *sects =
      (amx2_section_t *)malloc(header.section_count * sizeof(amx2_section_t));
  if (!sects) {
    free(pass1);
    return -1;
  }

  if (amx2_read_sections(pass1, &header, sects, header.section_count) != 0) {
    free(sects);
    free(pass1);
    return -1;
  }

  uint32_t max_file_end = pass1_size;

  for (uint32_t i = 0; i < header.section_count; i++) {
    uint32_t end = sects[i].file_offset + sects[i].file_size;
    if (end > max_file_end) {
      max_file_end = end;
    }
  }

  if (header.reloc_count > 0) {
    uint32_t end = header.reloc_offset + header.reloc_count * AMX2_RELOC_SIZE;
    if (end > max_file_end) {
      max_file_end = end;
    }
  }

  if (header.import_count > 0) {
    uint32_t end =
        header.import_offset + header.import_count * AMX2_IMPORT_SIZE;
    if (end > max_file_end) {
      max_file_end = end;
    }
  }

  if (header.export_count > 0) {
    uint32_t end =
        header.export_offset + header.export_count * AMX2_EXPORT_SIZE;
    if (end > max_file_end) {
      max_file_end = end;
    }
  }

  free(sects);
  free(pass1);

  /* Pass 2: read full file */
  uint8_t *buf = (uint8_t *)malloc(max_file_end);
  if (!buf) {
    return -1;
  }

  len = amfs_read(path, (char *)buf, max_file_end);
  if (len < (int)max_file_end) {
    free(buf);
    return -1;
  }

  *out_buf = buf;
  *out_size = max_file_end;

  return 0;
}

/* ==========================================================================
 *                                                                          *
 * exec2_load()                                                             *
 *                                                                          *
 * Reads AMX2 file from disk, parses header and sections.                     *
 * Validates everything. Fills context.                                     *
 * Returns 0 on success, -1 on failure.                                     *
 *                                                                          *
 * ======================================================================== */
int exec2_load(const char *path, exec2_context_t *ctx) {
  if (!path || !ctx) {
    return -1;
  }

  memset(ctx, 0, sizeof(exec2_context_t));

  uint8_t *buf = NULL;
  uint32_t size = 0;

  if (read_file(path, &buf, &size) != 0) {
    return -1;
  }

  if (amx2_read_header(buf, &ctx->header) != 0) {
    free(buf);
    return -1;
  }

  if (amx2_verify_header(&ctx->header) != AMX2_OK) {
    free(buf);
    return -1;
  }

  if (ctx->header.section_count > 0) {
    ctx->sections = (amx2_section_t *)malloc(ctx->header.section_count *
                                             sizeof(amx2_section_t));
    if (!ctx->sections) {
      free(buf);
      return -1;
    }

    if (amx2_read_sections(buf, &ctx->header, ctx->sections,
                           ctx->header.section_count) != 0) {
      free(ctx->sections);
      free(buf);
      return -1;
    }

    if (amx2_verify_sections(&ctx->header, ctx->sections) != AMX2_OK) {
      free(ctx->sections);
      free(buf);
      return -1;
    }
  }

  amx2_print_header(&ctx->header);

  for (uint32_t i = 0; i < ctx->header.section_count; i++) {
    amx2_print_section(&ctx->sections[i], i);
  }

  ctx->file_image = buf;
  ctx->file_size = size;
  ctx->image_base = NULL;
  ctx->stack_base = NULL;
  ctx->import_table = NULL;

  return 0;
}

/* ==========================================================================
 *                                                                          *
 * exec2_map()                                                              *
 *                                                                          *
 * Allocates memory for the image and maps sections.                        *
 * BSS sections are zero-filled.                                            *
 * Stack is allocated separately (4KB fixed for v2).                          *
 * Returns 0 on success, -1 on failure.                                     *
 *                                                                          *
 * ======================================================================== */
int exec2_map(exec2_context_t *ctx) {
  if (!ctx || !ctx->file_image) {
    return -1;
  }

  uint32_t image_size = ctx->header.image_size;

  /* TODO: When virtual memory is fully wired, try preferred_base first.
   * For now, malloc gives us whatever the heap has. */
  ctx->image_base = malloc(image_size);
  if (!ctx->image_base) {
    return -1;
  }

  memset(ctx->image_base, 0, image_size);

  for (uint32_t i = 0; i < ctx->header.section_count; i++) {
    amx2_section_t *s = &ctx->sections[i];

    if (s->file_size == 0) {
      continue;
    }

    if (s->file_offset + s->file_size > ctx->file_size) {
      free(ctx->image_base);
      ctx->image_base = NULL;
      return -1;
    }

    void *dest = (uint8_t *)ctx->image_base + s->vaddr;
    void *src = ctx->file_image + s->file_offset;

    memcpy(dest, src, s->file_size);
  }

  /* Fixed 4KB stack for v2 */
  ctx->stack_base = malloc(EXEC2_STACK_SIZE);
  if (!ctx->stack_base) {
    free(ctx->image_base);
    ctx->image_base = NULL;
    return -1;
  }

  return 0;
}

/* ==========================================================================
 *                                                                          *
 * sect_ptr()                                                               *
 *                                                                          *
 * Converts a virtual address (RVA) to a pointer in the mapped image.       *
 *                                                                          *
 * ======================================================================== */
static void *sect_ptr(exec2_context_t *ctx, uint32_t vaddr) {
  if (!ctx || !ctx->image_base) {
    return NULL;
  }

  if (vaddr >= ctx->header.image_size) {
    return NULL;
  }

  return (uint8_t *)ctx->image_base + vaddr;
}

/* ==========================================================================
 *                                                                          *
 * exec2_relocate()                                                         *
 *                                                                          *
 * Applies relocations from the relocation table.                           *
 * Returns 0 on success, -1 on failure.                                     *
 *                                                                          *
 * ======================================================================== */
int exec2_relocate(exec2_context_t *ctx) {
  if (!ctx || !ctx->image_base || !ctx->file_image) {
    return -1;
  }

  if (ctx->header.reloc_count == 0) {
    return 0;
  }

  if (ctx->header.reloc_offset + ctx->header.reloc_count * AMX2_RELOC_SIZE >
      ctx->file_size) {
    return -1;
  }

  amx2_reloc_t *table =
      (amx2_reloc_t *)(ctx->file_image + ctx->header.reloc_offset);

  for (uint32_t i = 0; i < ctx->header.reloc_count; i++) {
    amx2_reloc_t *r = &table[i];

    if (r->section_idx >= ctx->header.section_count) {
      printk("[AMX2] bad reloc section idx: %u\n", r->section_idx);
      return -1;
    }

    amx2_section_t *sect = &ctx->sections[r->section_idx];

    if (r->offset >= sect->mem_size) {
      printk("[AMX2] bad reloc offset: %u\n", r->offset);
      return -1;
    }

    if (r->offset + sizeof(uint32_t) > sect->mem_size) {
      printk("[AMX2] reloc overflows section\n");
      return -1;
    }

    uint32_t vaddr = sect->vaddr + r->offset;
    uint32_t *addr = (uint32_t *)sect_ptr(ctx, vaddr);

    if (!addr) {
      return -1;
    }

    switch (r->type) {
    case AMX2_RELOC_ABS32: {
      *addr += (uint32_t)(uintptr_t)ctx->image_base;
      break;
    }

    case AMX2_RELOC_REL32: {
      *addr += (uint32_t)(uintptr_t)ctx->image_base;
      *addr -= sect->vaddr;
      break;
    }

    case AMX2_RELOC_IMPORT: {
      if (!ctx->import_table) {
        printk("[AMX2] imports not resolved yet\n");
        return -1;
      }

      uint32_t import_idx = r->info;
      if (import_idx >= ctx->header.import_count) {
        printk("[AMX2] bad import idx: %u\n", import_idx);
        return -1;
      }

      *addr = (uint32_t)(uintptr_t)ctx->import_table[import_idx];
      break;
    }

    default: {
      printk("[AMX2] unknown reloc type: %u\n", r->type);
      return -1;
    }
    }
  }

  return 0;
}

/* ==========================================================================
 *                                                                          *
 * exec2_resolve_imports()                                                  *
 *                                                                          *
 * Resolves all imports by looking up module/symbol names.                    *
 * Fills ctx->import_table with function pointers.                          *
 * Returns 0 on success, -1 on failure.                                     *
 *                                                                          *
 * For v2: only "kernel" module is recognized, as a stub for future DLLs.     *
 * Syscalls use int 0x80 — imports are for inter-module linking.              *
 *                                                                          *
 * ======================================================================== */
int exec2_resolve_imports(exec2_context_t *ctx) {
  if (!ctx || !ctx->file_image) {
    return -1;
  }

  if (ctx->header.import_count == 0) {
    return 0;
  }

  if (ctx->header.import_offset + ctx->header.import_count * AMX2_IMPORT_SIZE >
      ctx->file_size) {
    return -1;
  }

  if (ctx->header.string_offset + ctx->header.string_size > ctx->file_size) {
    return -1;
  }

  ctx->import_table =
      (void **)malloc(ctx->header.import_count * sizeof(void *));
  if (!ctx->import_table) {
    return -1;
  }

  amx2_import_t *imports =
      (amx2_import_t *)(ctx->file_image + ctx->header.import_offset);
  char *strings = (char *)(ctx->file_image + ctx->header.string_offset);

  for (uint32_t i = 0; i < ctx->header.import_count; i++) {
    amx2_import_t *imp = &imports[i];

    if (imp->module_name >= ctx->header.string_size) {
      goto fail;
    }

    if (imp->symbol_name >= ctx->header.string_size) {
      goto fail;
    }

    const char *module = strings + imp->module_name;
    const char *symbol = strings + imp->symbol_name;

    printk("[AMX2] Import %u: %s.%s\n", i, module, symbol);

    /* TODO: Real module loader. For now, only "kernel" is known. */
    if (strcmp(module, "kernel") == 0) {
      /* Placeholder: kernel exports will be registered later */
      ctx->import_table[i] = NULL;
      printk("[AMX2]   -> kernel stub (unresolved)\n");
    } else {
      printk("[AMX2]   -> unknown module: %s\n", module);
      goto fail;
    }
  }

  return 0;

fail:
  free(ctx->import_table);
  ctx->import_table = NULL;
  return -1;
}

/* ==========================================================================
 *                                                                          *
 * exec2_start()                                                            *
 *                                                                          *
 * Sets up stack pointer and transfers control to the loaded program.         *
 * Returns 0 on success (program returned), or -1 on error.                 *
 *                                                                          *
 * ======================================================================== */
int exec2_start(exec2_context_t *ctx) {
  if (!ctx || !ctx->image_base) {
    return -1;
  }

  uint32_t entry_vaddr = ctx->header.entry;

  if (entry_vaddr >= ctx->header.image_size) {
    return -1;
  }

  void (*entry)(void) =
      (void (*)(void))((uintptr_t)ctx->image_base + entry_vaddr);

  /* Stack grows down. ESP points to top of allocated block. */
  uint32_t stack_top = (uint32_t)(uintptr_t)ctx->stack_base + EXEC2_STACK_SIZE;

  printk("[AMX2] Jumping to entry at 0x%x (vaddr 0x%08x)\n",
         (uint32_t)(uintptr_t)entry, entry_vaddr);
  printk("[AMX2] Stack top: 0x%08x\n", stack_top);

  /* Switch to program stack and jump.
   * We save/restore EBP so the C compiler isn't confused on return.
   * This is the same pattern PE loader uses in ntdll.dll. */
  __asm__ __volatile__("mov %%ebp, %%edx\n\t" /* Save frame pointer */
                       "mov %0, %%esp\n\t"    /* Switch to program stack */
                       "call *%1\n\t"         /* Call entry point */
                       "mov %%edx, %%esp\n\t" /* Restore kernel stack */
                       "mov %%edx, %%ebp\n\t" /* Restore frame pointer */
                       :
                       : "r"(stack_top), "r"(entry)
                       : "eax", "ecx", "edx", "memory");

  printk("[AMX2] Program returned\n");

  return 0;
}

/* ==========================================================================
 *                                                                          *
 * exec2_cleanup()                                                          *
 *                                                                          *
 * Frees all resources associated with an execution context.                 *
 * Safe to call even if load failed partway through.                        *
 *                                                                          *
 * ======================================================================== */
void exec2_cleanup(exec2_context_t *ctx) {
  if (!ctx) {
    return;
  }

  if (ctx->import_table) {
    free(ctx->import_table);
    ctx->import_table = NULL;
  }

  if (ctx->stack_base) {
    free(ctx->stack_base);
    ctx->stack_base = NULL;
  }

  if (ctx->image_base) {
    free(ctx->image_base);
    ctx->image_base = NULL;
  }

  if (ctx->sections) {
    free(ctx->sections);
    ctx->sections = NULL;
  }

  if (ctx->file_image) {
    free(ctx->file_image);
    ctx->file_image = NULL;
  }

  ctx->file_size = 0;
  memset(&ctx->header, 0, sizeof(amx2_header_t));
}

/* ==========================================================================
 *                                                                          *
 * exec2_run()                                                              *
 *                                                                          *
 * Load -> Map -> Resolve Imports -> Relocate -> Start -> Cleanup           *
 * Returns 0 on success, -1 on failure.                                     *
 *                                                                          *
 * ======================================================================== */
int exec2_run(const char *path) {
  exec2_context_t ctx;

  if (exec2_load(path, &ctx) != 0) {
    return -1;
  }

  if (exec2_map(&ctx) != 0) {
    goto fail;
  }

  if (exec2_resolve_imports(&ctx) != 0) {
    goto fail;
  }

  if (exec2_relocate(&ctx) != 0) {
    goto fail;
  }

  int ret = exec2_start(&ctx);

  exec2_cleanup(&ctx);
  return ret;

fail:
  exec2_cleanup(&ctx);
  return -1;
}