#include "vfs.h"
#include "ramfs.h"

void fs_init() {
    fs_node_t *root = ramfs_create_dir("");
    fs_node_t *saved = ramfs_create_dir("Saved");

    ramfs_add_child(root, saved);
    ramfs_add_child(saved, ramfs_create_file("hello.txt", "Hello from AmitX!\n"));
    ramfs_add_child(saved, ramfs_create_file("log.txt", "System log started\n"));

    vfs_mount(root);
}