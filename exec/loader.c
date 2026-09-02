/*
 * exec/loader.c - [Enter description]
 * Author:   amity
 * Date:     Mon Jun 29 12:57:18 2026
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

/* --- Macros ---*/

/* --- Includes ---*/
#include <exec/amx.h>
#include <exec/loader.h>
#include <fs/amfs.h>
#include <lib/string.h>
#include <mm/heap.h> /* mem... functions live in heap.c, move to string.c later */
#include <screen/printk.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/

/* --- Functions ---*/

/* ==========================================================================
 *                                                                          *
 * exec_load()                                                              *
 *                                                                          *
 * Reads AMX file from disk                                                 *
 * Parses and validates header                                              *
 * Fills context                                                            *
 * Returns 0 on success.                                                    *
 *                                                                          *
 ========================================================================== */
int exec_load(const char *path, exec_context_t *ctx) {
  /* Validate path */
  if (!path || path[0] != '/')
    return -1; /* Give a path at least */
  if (strcmp(path, "/") == 0)
    return -1; /* You can't create root */
  if (!amfs_exists(path))
    return -1; /* File doesn't exist yet */

  /* Read just the header first */
  uint8_t hdr_buf[AMX_HEADER_SIZE];
  int len = amfs_read(path, (char *)hdr_buf, sizeof(hdr_buf));
  if (len < AMX_HEADER_SIZE)
    return -1;

  amx_header_t header;
  if (read_header(hdr_buf, &header) != 0)
    return -1;
  if (verify_header(&header) != AMX_OK)
    return -1;

  /* Now allocate the right size */
  uint32_t total = header.image_offset + header.image_size +
                   header.reloc_count * sizeof(amx_reloc_t);
  uint8_t *buf = (uint8_t *)malloc(total);
  if (!buf)
    return -1;

  /* Read the whole file */
  len = amfs_read(path, (char *)buf, total);
  if (len < (int)total) {
    free(buf);
    return -1;
  }

  /* Display header to user */
  print_header(&header);

  ctx->header = header;
  ctx->file_image = buf;
  ctx->image_base = NULL;
  ctx->stack = NULL;

  return 0;
}

/* ==========================================================================
 *                                                                          *
 * exec_map()                                                               *
 *                                                                          *
 * Converts file to memory layout                                           *
 * Allocates heap memory                                                    *
 * Copies image + sets up BSS/Stack                                         *
 * Returns 0 on success.                                                    *
 *                                                                          *
 ========================================================================== */
int exec_map(exec_context_t *ctx) {
  ctx->image_base = malloc(ctx->header.image_size + ctx->header.bss_size);
  if (!ctx->image_base) {
    return -1;
  }

  memcpy(ctx->image_base, ctx->file_image + ctx->header.image_offset,
         ctx->header.image_size);

  ctx->stack = malloc(ctx->header.stack_size);
  if (!ctx->stack) {
    free(ctx->image_base);
    ctx->image_base = NULL;
    return -1;
  }

  memset((uint8_t *)ctx->image_base + ctx->header.image_size, 0,
         ctx->header.bss_size);
  return 0;
}

static inline void *image_ptr(exec_context_t *ctx, uint32_t offset) {
  return (uint8_t *)ctx->image_base + offset;
}

/* ==========================================================================
 *                                                                          *
 * exec_relocate()                                                          *
 *                                                                          *
 * Relocate offset variables                                                *
 * Reads relocation table                                                   *
 * Returns 0 on success.                                                    *
 *                                                                          *
 ========================================================================== */

int exec_relocate(exec_context_t *ctx) {
  if (!ctx || !ctx->image_base || !ctx->file_image) {
    return -1;
  }

  amx_reloc_t *table =
      (amx_reloc_t *)(ctx->file_image + ctx->header.reloc_offset);

  for (uint32_t i = 0; i < ctx->header.reloc_count; i++) {
    amx_reloc_t *r = &table[i];
    if (r->offset >= ctx->header.image_size) {
      printk("bad reloc offset: %u\n", r->offset);
      return -1;
    }

    if (r->offset + sizeof(uint32_t) > ctx->header.image_size) {
      return -1;
    }

    switch (r->type) {
    case AMX_RELOC_ABS32: {
      uint32_t *addr = image_ptr(ctx, r->offset);

      *addr += (ULONG_PTR)ctx->image_base;

      break;
    }
    default: {
      return -1;
    }
    }
  }

  return 0;
}

/* ==========================================================================
 *                                                                          *
 * exec_start()                                                             *
 *                                                                          *
 * Transfer control                                                         *
 * Set IP, switch stack, jump to ring 3 (userspace)                         *
 * Returns 0 on success.                                                    *
 *                                                                          *
 ========================================================================== */
int exec_start(exec_context_t *ctx) {
  if (!ctx || !ctx->image_base)
    return -1;

  /* Entry point is offset from image_base */
  void (*entry)(void) =
      (void (*)(void))((ULONG_PTR)ctx->image_base + ctx->header.entry);

  printk("[AMX] Jumping to entry at 0x%x\n", ctx->header.entry);

  entry();

  printk("[AMX] Program returned\n");

  return 0;
}

/* ==========================================================================
 *                                                                          *
 * exec_cleanup()                                                           *
 *                                                                          *
 * Clean up execution context                                               *
 *                                                                          *
 ========================================================================== */
void exec_cleanup(exec_context_t *ctx) {
  if (ctx->stack) {
    free(ctx->stack);
    ctx->stack = NULL;
  }
  if (ctx->image_base) {
    free(ctx->image_base);
    ctx->image_base = NULL;
  }
  if (ctx->file_image) {
    free(ctx->file_image);
    ctx->file_image = NULL;
  }
}

/* ==========================================================================
 *                                                                          *
 * exec_run()                                                               *
 *                                                                          *
 * Load -> Map -> Relocate -> Start -> Cleanup                              *
 * Returns 0 on success                                                     *
 *                                                                          *
 ========================================================================== */
int exec_run(const char *path) {
  exec_context_t ctx;

  if (exec_load(path, &ctx) != 0) {
    return -1;
  }

  if (exec_map(&ctx) != 0) {
    goto fail;
  }

  if (exec_relocate(&ctx) != 0) {
    goto fail;
  }

  int ret = exec_start(&ctx);

  exec_cleanup(&ctx);
  return ret;

fail:
  exec_cleanup(&ctx);
  return -1;
}