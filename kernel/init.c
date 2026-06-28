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
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include "shell/commands.h"
#include <gfx/svga.h>
#include <screen/screen.h>
#include <arch/x86/io.h>
#include <fs/amfs.h>
#include <drivers/serial.h>
#include <arch/x86/timer.h>
#include <drivers/keyboard.h>
#include <hw/acpi.h>
#include <mm/pmm.h>
#include <internal/multiboot.h>
#include <arch/x86/interrupts.h>
#include <mm/paging.h>
#include <hw/pci.h>
#include <hw/ide.h>
#include <arch/x86/gdt.h>
#include <hw/e1000.h>
#include <mm/heap.h>
#include <arch/x86/idt.h>
#include <kernel/syscall.h>
#include <screen/printk.h>
#include <drivers/mouse.h>
#include <kernel/kernel.h>
#include <internal/amitx_consts.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
extern uint32_t multiboot_info_ptr;
/* --- Prototypes ---*/

/* --- Functions ---*/
static int storage_init(void) {
    pci_device_t *dev = pci_get_first_device();
    int found_ide = 0;
    int fs_mounted = 0;

    while (dev) {
        if (dev->class_code != PCI_CLASS_MASS_STORAGE) {
            dev = dev->next;
            continue;
        }

        printk("[storage] Found $%02x:%02x.%x  %s\n", dev->bus, dev->device, dev->function, pci_subclass_name(dev->class_code, dev->subclass));

        switch (dev->subclass) {
            case 0x00: { /* SCSI Bus */
                printk("[storage] SCSI controller detected — driver not yet implemented\n");
                break;
            }
            case 0x01: {  /* IDE */
                if (found_ide)
                    break;
            
                /* QEMU disk is on primary channel */
                ide_init(IDE_PRIMARY_DATA, IDE_PRIMARY_CTRL);
            
                uint16_t identify[256];
                int present_drive = -1;
            
                for (int drive = 0; drive < 2; drive++) {
                    if (ide_identify(drive, identify) == 0) {
                        present_drive = drive;
                        break;
                    }
                }
            
                if (present_drive < 0) {
                    printk("[storage] No drive on primary channel\n");
                    break;
                }
            
                found_ide = 1;
            
                /* Try to mount existing filesystem */
                if (amfs_mount() == 0) {
                    printk("[storage] AmFS mounted from existing image\n");
                    fs_mounted = 1;
                    break;
                }
            
                printk("[storage] No valid AmFS, formatting...\n");
                if (amfs_mkfs(10 * 2048) != 0) {
                    printk("[storage] amfs_mkfs failed\n");
                    break;
                }
            
                if (amfs_mount() != 0) {
                    printk("[storage] amfs_mount failed after mkfs\n");
                    break;
                }
            
                if (amfs_write("/helloworld.txt", "Hello from AmitFS!\n", 19) != 0) {
                    printk("[storage] Failed to write /helloworld.txt\n");
                }
                if (amfs_write("/README", "AmitX Filesystem v0.1\n", 22) != 0) {
                    printk("[storage] Failed to write /README\n");
                }
            
                fs_mounted = 1;
                break;
            }
            case 0x02: { /* Floppy Disk */
                printk("[storage] Floppy Disk controller detected — driver not yet implemented\n");
                break;
            }
            case 0x03: { /* IPI Bus */
                printk("[storage] IPI Bus controller detected — driver not yet implemented\n");
                break;
            }
            case 0x04: { /* RAID */
                printk("[storage] RAID controller detected — driver not yet implemented\n");
                break;
            }
            case 0x05: { /* ATA */
                printk("[storage] ATA controller detected — driver not yet implemented\n");
                break;
            }
            case 0x06: { /* SATA */
                printk("[storage] SATA controller detected —  AHCI driver not yet implemented\n");
                break;
            }
            case 0x07: { /* SAS */
                printk("[storage] SAS controller detected — driver not yet implemented\n");
                break;
            }
            case 0x08: { /* NVMe */
                printk("[storage] NVMe controller detected — driver not yet implemented\n");
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
        amfs_ls("/");

        char buf[256];
        int len;

        len = amfs_read("/helloworld.txt", buf, sizeof(buf));
        if (len > 0) {
            buf[len] = '\0';
            printk("[amfs] Read back: %s", buf);
        }

        len = amfs_read("/README", buf, sizeof(buf));
        if (len > 0) {
            buf[len] = '\0';
            printk("[amfs] Read back: %s", buf);
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
    .requires = (kscope_node_t*[]){&pci_node, &heap_node},
    .require_count = 2,
    .provides = (const char*[]){"storage.block", "fs.amfs"},
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

    pos += ksnprintf(buf + pos, 4096 - pos,
        "=== PCI Device Inventory ===\n\n");

    while (dev) {
        pos += ksnprintf(buf + pos, 4096 - pos,
            "%02x:%02x.%x  %04x:%04x  %s (%s)  Rev %02x  IRQ %u\n",
            dev->bus, dev->device, dev->function,
            dev->vendor_id, dev->device_id,
            pci_class_name(dev->class_code),
            pci_subclass_name(dev->class_code, dev->subclass),
            dev->revision, dev->interrupt_line);

        for (uint8_t i = 0; i < dev->bar_count; i++) {
            if (dev->bars[i].base == 0 && dev->bars[i].size == 0)
                continue;

            if (dev->bars[i].is_io) {
                pos += ksnprintf(buf + pos, 4096 - pos,
                    "  BAR%d: I/O  0x%08x  size=0x%x\n",
                    i, dev->bars[i].base, dev->bars[i].size);
            } else {
                pos += ksnprintf(buf + pos, 4096 - pos,
                    "  BAR%d: MEM  0x%08x  size=0x%x  %s\n",
                    i, dev->bars[i].base, dev->bars[i].size,
                    dev->bars[i].is_prefetch ? "prefetchable" : "");
            }
        }
        pos += ksnprintf(buf + pos, 4096 - pos, "\n");
        dev = dev->next;
    }

    pos += ksnprintf(buf + pos, 4096 - pos,
        "=== End of Inventory ===\n");

    amfs_write("/pci_devices.txt", buf, pos);
    free(buf);
}

void kernel_setup(void) {
    kscope_register_all();
    kscope_probe_all();
    paging_init();
    kscope_dump();

    syscall_init();

    pmm_print_map();
	pci_print_all_devices();
    acpi_print_info();
    if (amfs_is_mounted()) {
        pci_log_to_fs();
        kscope_log_to_fs();
    }


    puts("\nPress ENTER to continue...");
    while (keyboard_getchar() != '\n');

    clear();

    svga_init();
    execute_command("run /etc/rc");
}