/*
 * kernel/init.c - [Enter description]
 * Author:   amity
 * Date:     Wed Jun 10 10:32:15 2026
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
#include "shell/commands.h"
#include <arch/x86/cpuid.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <arch/x86/interrupts.h>
#include <arch/x86/io.h>
#include <arch/x86/timer.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/serial.h>
#include <fs/amfs.h>
#include <fs/smkfs.h>
#include <gfx/fb.h>
#include <gfx/gfx_term.h>
#include <hw/acpi.h>
#include <hw/e1000.h>
#include <hw/ide.h>
#include <hw/pci.h>
#include <hw/svga.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <internal/multiboot.h>
#include <internal/phonon_consts.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <mm/paging.h>
#include <mm/pmm.h>
#include <screen/printk.h>
#include <screen/screen.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern uint32_t multiboot_info_ptr;
extern cpu_info_t info;
extern void c3_test(void);
/* --- Prototypes ---*/

/* --- Functions ---*/
static int storage_init(void) {
  pci_device_t *dev = pci_get_first_device();
  int found_ide = 0;
  int fs_mounted = 0;
  char drive_letter = 'A';
  _SMKFS_MOUNT *mnt = malloc(sizeof(_SMKFS_MOUNT));
  if (!mnt) {
    printk("[storage] Failed to allocate mount context\n");
    return -1;
  }
  memset(mnt, 0, sizeof(_SMKFS_MOUNT));

  while (dev) {
    if (dev->class_code != PCI_CLASS_MASS_STORAGE) {
      dev = dev->next;
      continue;
    }

    printk("[storage] Found $%02x:%02x.%x  %s\n", dev->bus, dev->device,
           dev->function, pci_subclass_name(dev->class_code, dev->subclass));

    switch (dev->subclass) {
    case 0x00: { /* SCSI Bus */
      printk(
          "[storage] SCSI controller detected — driver not yet implemented\n");
      break;
    }
    case 0x01: { /* IDE */
      if (found_ide)
        break;

      /* QEMU disk is on primary channel
       * Use PCI-assigned I/O BARs when available so we don't
       * clobber legacy port mapping by reassigning BARs.
       */
      if (dev->bars[0].is_io && dev->bars[0].base != 0 && dev->bars[1].is_io &&
          dev->bars[1].base != 0) {
        ide_init((uint16_t)dev->bars[0].base, (uint16_t)dev->bars[1].base);
      } else {
        /* Fallback to legacy ports when PCI BARs are not usable */
        ide_init(IDE_PRIMARY_DATA, IDE_PRIMARY_CTRL);
      }

      ide_identify_t identify;
      int present_drive = -1;

      for (int drive = 0; drive < 2; drive++) {
        if (ide_identify(drive, &identify) == 0) {
          present_drive = drive;
          break;
        }
      }

      ide_dump_identify(&identify);

      if (present_drive < 0) {
        printk("[storage] No drive on primary channel\n");
        break;
      }

      found_ide = 1;
      drive_letter = (char)('A' + present_drive);
      (void)drive_letter;

      /* Try to mount existing filesystem */
      if (smkfs_mount((uint8_t)present_drive, mnt) == 0) {
        printk("[storage] SmKFS mounted from existing image\n");
        fs_mounted = 1;
        break;
      }

      printk("[storage] No valid SmKFS, formatting...\n");
      uint64_t total_sectors;

      if (ide_supports_lba48(&identify)) {
        total_sectors = identify.total_lba48_sectors;
      } else {
        total_sectors = identify.total_lba28_sectors;
      }

      uint64_t total_blocks =
          total_sectors / (SMKFS_BLOCK_SIZE / identify.logical_sz); // 2560
      if (smkfs_mkfs((uint8_t)present_drive, total_blocks,
                     identify.logical_sz) != SMKFS_OK) {
        printk("[storage] smkfs_mkfs failed\n");
        break;
      }

      if (smkfs_mount((uint8_t)present_drive, mnt) != 0) {
        printk("[storage] smkfs_mount failed after mkfs\n");
        break;
      }

      if (smkfs_mkdir(mnt, "A:/docs") != SMKFS_OK) {
        printk("mkdir failed\n");
      }

      printk("created directory\n");

      char path_hello[20];
      char path_readme[16];
      char path_nested[24];
      path_hello[0] = drive_letter;
      path_hello[1] = ':';
      path_hello[2] = '/';
      strcpy(path_hello + 3, "helloworld.txt");

      path_readme[0] = drive_letter;
      path_readme[1] = ':';
      path_readme[2] = '/';
      strcpy(path_readme + 3, "README");

      path_nested[0] = drive_letter;
      path_nested[1] = ':';
      path_nested[2] = '/';
      strcpy(path_nested + 3, "docs/nested.txt");

      int fd = smkfs_open(mnt, path_hello, SMKFS_O_WRONLY | SMKFS_O_CREATE);
      SMKFS_STATUS err = smkfs_write_file(mnt, fd, "Hello from SmKFS!\n", 19);
      if (fd < 0 || err < 0) {
        printk("[storage] Failed to write %s\n", path_hello);
      }
      if (fd >= 0)
        smkfs_close(mnt, fd);

      fd = smkfs_open(mnt, path_readme, SMKFS_O_WRONLY | SMKFS_O_CREATE);
      if (fd < 0 || smkfs_write_file(
                        mnt, fd, "Shadow Kernel File System v0.1\n", 32) < 0) {
        printk("[storage] Failed to write %s\n", path_readme);
      }
      if (fd >= 0)
        smkfs_close(mnt, fd);

      fd = smkfs_open(mnt, path_nested, SMKFS_O_WRONLY | SMKFS_O_CREATE);
      if (fd < 0 || smkfs_write_file(mnt, fd, "nested file\n", 12) < 0) {
        printk("[storage] Failed to write %s\n", path_nested);
      }
      if (fd >= 0)
        smkfs_close(mnt, fd);

      smkfs_rmdir(mnt, "A:/docs");

      fs_mounted = 1;
      break;
    }
    case 0x02: { /* Floppy Disk */
      printk("[storage] Floppy Disk controller detected — driver not yet "
             "implemented\n");
      break;
    }
    case 0x03: { /* IPI Bus */
      printk("[storage] IPI Bus controller detected — driver not yet "
             "implemented\n");
      break;
    }
    case 0x04: { /* RAID */
      printk(
          "[storage] RAID controller detected — driver not yet implemented\n");
      break;
    }
    case 0x05: { /* ATA */
      printk(
          "[storage] ATA controller detected — driver not yet implemented\n");
      break;
    }
    case 0x06: { /* SATA */
      printk("[storage] SATA controller detected —  AHCI driver not yet "
             "implemented\n");
      break;
    }
    case 0x07: { /* SAS */
      printk(
          "[storage] SAS controller detected — driver not yet implemented\n");
      break;
    }
    case 0x08: { /* NVMe */
      printk(
          "[storage] NVMe controller detected — driver not yet implemented\n");
      break;
    }
    default: {
      printk("[storage] Subclass 0x%02x not yet supported\n", dev->subclass);
      break;
    }
    }
    dev = dev->next;
  }
  if (!found_ide) {
    printk("[storage] No supported mass-storage controller found\n");
  }

  if (fs_mounted) {
    char root_path[4] = {drive_letter, ':', '/', '\0'};
    _SMKFS_DIRENT entries[32];
    size_t count = 0;

    if (smkfs_readdir(mnt, root_path, entries, 32, &count) == 0) {
      printk("[storage] Root directory (%zu entries):\n", count);
      for (size_t i = 0; i < count; i++) {
        printk("  %s\n", entries[i].name);
      }
    } else {
      printk("[storage] smkfs_readdir failed on %s\n", root_path);
    }

    if (smkfs_readdir(mnt, "A:/docs", entries, 32, &count) == 0) {
      printk("[storage] Docs directory (%zu entries):\n", count);
      for (size_t i = 0; i < count; i++) {
        printk("  %s\n", entries[i].name);
      }
    } else {
      printk("[storage] smkfs_readdir failed on %s\n", root_path);
    }

    char path_hello[20];
    char path_hello2[24];
    char path_readme[16];
    char path_nested[24];
    path_hello[0] = drive_letter;
    path_hello[1] = ':';
    path_hello[2] = '/';
    strcpy(path_hello + 3, "helloworld.txt");

    path_hello2[0] = drive_letter;
    path_hello2[1] = ':';
    path_hello2[2] = '/';
    strcpy(path_hello2 + 3, "docs/helloworld.txt");

    path_readme[0] = drive_letter;
    path_readme[1] = ':';
    path_readme[2] = '/';
    strcpy(path_readme + 3, "README");

    path_nested[0] = drive_letter;
    path_nested[1] = ':';
    path_nested[2] = '/';
    strcpy(path_nested + 3, "docs/nested.txt");

    char buf[256];
    int len;
    int fd;

    fd = smkfs_open(mnt, path_hello, SMKFS_O_RDONLY);
    if (fd >= 0) {
      len = smkfs_read_file(mnt, fd, buf, sizeof(buf) - 1);
      printk("len = %d\n", len);
      if (len > 0) {
        buf[len] = '\0';
        printk("[storage] Read back %s: %s\n", path_hello, buf);
      } else {
        printk("[storage] Read back %s: error\n", path_hello);
      }
      smkfs_close(mnt, fd);
    } else {
      printk("[storage] Failed to open %s for readback\n", path_hello);
    }

    smkfs_link(mnt, path_hello, path_hello2);
    fd = smkfs_open(mnt, path_hello2, SMKFS_O_RDONLY);
    if (fd >= 0) {
      len = smkfs_read_file(mnt, fd, buf, sizeof(buf) - 1);
      printk("len = %d\n", len);
      if (len > 0) {
        buf[len] = '\0';
        printk("[storage] Read back %s: %s\n", path_hello2, buf);
      } else {
        printk("[storage] Read back %s: error\n", path_hello2);
      }
      smkfs_close(mnt, fd);
    } else {
      printk("[storage] Failed to open %s for readback\n", path_hello2);
    }

    fd = smkfs_open(mnt, path_readme, SMKFS_O_RDONLY);
    if (fd >= 0) {
      len = smkfs_read_file(mnt, fd, buf, sizeof(buf) - 1);
      printk("len = %d\n", len);
      if (len > 0) {
        buf[len] = '\0';
        printk("[storage] Read back %s: %s\n", path_readme, buf);
      } else {
        printk("[storage] Read back %s: error\n", path_readme);
      }
      smkfs_close(mnt, fd);
    } else {
      printk("[storage] Failed to open %s for readback\n", path_readme);
    }

    fd = smkfs_open(mnt, path_nested, SMKFS_O_RDONLY);
    if (fd >= 0) {
      len = smkfs_read_file(mnt, fd, buf, sizeof(buf) - 1);
      printk("len = %d\n", len);
      if (len > 0) {
        buf[len] = '\0';
        printk("[storage] Read back %s: %s\n", path_nested, buf);
      } else {
        printk("[storage] Read back %s: error\n", path_nested);
      }
      smkfs_close(mnt, fd);
    } else {
      printk("[storage] Failed to open %s for readback\n", path_nested);
    }
  } else {
    printk("[storage] No filesystem available, skipping file ops\n");
  }
  return 0;
}

kscope_node_t storage_node = {
    .name = "storage",
    .id = 0x000D,
    .class = KSCOPE_CLASS_STORAGE,
    .subclass = KSCOPE_SUBCLASS_STORAGE_CONTROLLER,
    .requires = (kscope_node_t *[]){&pci_node, &heap_node},
    .require_count = 2,
    .provides = (const char *[]){"storage.block", "fs.amfs"},
    .provide_count = 2,
    .init = storage_init,
};

void pci_log_to_fs(void) {
  char *buf = (char *)malloc(4096);
  if (!buf) {
    printk("[pci_log] Failed to allocate buffer\n");
    return;
  }

  int pos = 0;
  pci_device_t *dev = pci_get_first_device();

  pos += ksnprintf(buf + pos, 4096 - pos, "=== PCI Device Inventory ===\n\n");

  while (dev) {
    pos += ksnprintf(buf + pos, 4096 - pos,
                     "%02x:%02x.%x  %04x:%04x  %s (%s)  Rev %02x  IRQ %u\n",
                     dev->bus, dev->device, dev->function, dev->vendor_id,
                     dev->device_id, pci_class_name(dev->class_code),
                     pci_subclass_name(dev->class_code, dev->subclass),
                     dev->revision, dev->interrupt_line);

    for (uint8_t i = 0; i < dev->bar_count; i++) {
      if (dev->bars[i].base == 0 && dev->bars[i].size == 0)
        continue;

      if (dev->bars[i].is_io) {
        pos += ksnprintf(buf + pos, 4096 - pos,
                         "  BAR%d: I/O  0x%08x  size=0x%x\n", i,
                         dev->bars[i].base, dev->bars[i].size);
      } else {
        pos += ksnprintf(buf + pos, 4096 - pos,
                         "  BAR%d: MEM  0x%08x  size=0x%x  %s\n", i,
                         dev->bars[i].base, dev->bars[i].size,
                         dev->bars[i].is_prefetch ? "prefetchable" : "");
      }
    }
    pos += ksnprintf(buf + pos, 4096 - pos, "\n");
    dev = dev->next;
  }

  pos += ksnprintf(buf + pos, 4096 - pos, "=== End of Inventory ===\n");

  amfs_write("/pci_devices.txt", buf, pos);
  free(buf);
}

void kernel_setup(void) {
  kscope_register_all();
  kscope_probe_all();

  if (!svga.initialized && fb_init() == 0) {
    gfx_term_init(0, 0, fb.back.width, fb.back.height,
                  gfx_theme_color(GFX_FG_TEXT),
                  gfx_theme_color(GFX_BG_DESKTOP));
    gfx_term_draw_frame("Phonon OS");
  }

  __asm__ __volatile__("sti");

  kscope_dump();
  c3_test();

  syscall_init();

  pmm_print_map();
  pci_print_all_devices();
  acpi_print_info();
  if (amfs_is_mounted()) {
    pci_log_to_fs();
    kscope_log_to_fs();
  }
  cpuid_dump(&info);
  puts("\nPress ENTER to continue...");
  while (keyboard_getchar() != '\n')
    ;

  if (svga_init() != 0) {
    printk("[gfx] SVGA initialization failed; keeping boot framebuffer\n");
  } else {
    gfx_term_init(0, 0, fb.back.width, fb.back.height,
                  gfx_theme_color(GFX_FG_TEXT),
                  gfx_theme_color(GFX_BG_DESKTOP));
    gfx_term_draw_frame("Phonon OS");
  }
  execute_command("run /etc/rc");
  clear();
}