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
#include <hw/svga.h>
#include <screen/screen.h>
#include <arch/x86/io.h>
#include <fs/amfs.h>
#include <fs/smkfs.h>
#include <lib/string.h>
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
#include <arch/x86/cpuid.h>
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
extern cpu_info_t info;
/* --- Prototypes ---*/

/* --- Functions ---*/
static int storage_init(void) {
    pci_device_t *dev = pci_get_first_device();
    int found_ide = 0;
    int fs_mounted = 0;
    char drive_letter = 'A';

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
                drive_letter = (char)('A' + present_drive);
                (void)drive_letter;
            
                /* Try to mount existing filesystem */
                if (smkfs_mount((uint8_t)present_drive) == 0) {
                    printk("[storage] SmKFS mounted from existing image\n");
                    fs_mounted = 1;
                    break;
                }
            
                printk("[storage] No valid SmKFS, formatting...\n");
                uint64_t total_blocks = (10ULL * 1024 * 1024) / SMKFS_BLOCK_SIZE; // 2560
                if (smkfs_mkfs((uint8_t)present_drive, total_blocks) != 0) {
                    printk("[storage] smkfs_mkfs failed\n");
                    break;
                }
            
                if (smkfs_mount((uint8_t)present_drive) != 0) {
                    printk("[storage] smkfs_mount failed after mkfs\n");
                    break;
                }
            
                char path_hello[SMKFS_NAME_LEN];
                char path_readme[SMKFS_NAME_LEN];
                path_hello[0] = drive_letter;
                path_hello[1] = ':';
                path_hello[2] = '/';
                strncpy(path_hello + 3, "helloworld.txt", SMKFS_NAME_LEN - 3);

                path_readme[0] = drive_letter;
                path_readme[1] = ':';
                path_readme[2] = '/';
                strncpy(path_readme + 3, "README", SMKFS_NAME_LEN - 3);

                int fd = smkfs_open(path_hello, SMKFS_O_WRONLY | SMKFS_O_CREATE);
                if (fd < 0 || smkfs_write_file(fd, "Hello from SmKFS!\n", 19) < 0) {
                    printk("[storage] Failed to write %s\n", path_hello);
                }
                if (fd >= 0) smkfs_close(fd);

                fd = smkfs_open(path_readme, SMKFS_O_WRONLY | SMKFS_O_CREATE);
                if (fd < 0 || smkfs_write_file(fd, "Shadow Kernel File System v0.1\n", 32) < 0) {
                    printk("[storage] Failed to write %s\n", path_readme);
                }
                if (fd >= 0) smkfs_close(fd);
            
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
        char root_path[4] = { drive_letter, ':', '/', '\0' };
        smkfs_dirent_t entries[32];
        size_t count = 0;

        if (smkfs_readdir(root_path, entries, 32, &count) == 0) {
            printk("[storage] Root directory (%zu entries):\n", count);
            for (size_t i = 0; i < count; i++) {
                printk("  %s\n", entries[i].name);
            }
        } else {
            printk("[storage] smkfs_readdir failed on %s\n", root_path);
        }

        char path_hello[SMKFS_NAME_LEN];
        char path_readme[SMKFS_NAME_LEN];
        path_hello[0] = drive_letter;
        path_hello[1] = ':';
        path_hello[2] = '/';
        strncpy(path_hello + 3, "helloworld.txt", SMKFS_NAME_LEN - 3);

        path_readme[0] = drive_letter;
        path_readme[1] = ':';
        path_readme[2] = '/';
        strncpy(path_readme + 3, "README", SMKFS_NAME_LEN - 3);

        char buf[256];
        int len;
        int fd;

        fd = smkfs_open(path_hello, SMKFS_O_RDONLY);
        if (fd >= 0) {
            len = smkfs_read_file(fd, buf, sizeof(buf) - 1);
            printk("len = %d\n", len);
            if (len > 0) {
                buf[len] = '\0';
                printk("[storage] Read back %s: %s\n", path_hello, buf);
            } else {
                printk("[storage] Read back %s: error\n", path_hello);
            }
            smkfs_close(fd);
        } else {
            printk("[storage] Failed to open %s for readback\n", path_hello);
        }

        fd = smkfs_open(path_readme, SMKFS_O_RDONLY);
        if (fd >= 0) {
            len = smkfs_read_file(fd, buf, sizeof(buf) - 1);
            printk("len = %d\n", len);
            if (len > 0) {
                buf[len] = '\0';
                printk("[storage] Read back %s: %s\n", path_readme, buf);
            } else {
                printk("[storage] Read back %s: error\n", path_readme);
            }
            smkfs_close(fd);
        } else {
            printk("[storage] Failed to open %s for readback\n", path_readme);
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
    kscope_dump();

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
    while (keyboard_getchar() != '\n');

    svga_init();
    execute_command("run /etc/rc");
    clear();
}