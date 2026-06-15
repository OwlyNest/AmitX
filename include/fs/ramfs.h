#ifndef RAMFS_H
#define RAMFS_H

#include "fs/vfs.h"
#include "mm/heap.h"

#define RAMFS_MAX_CHILDREN 16

typedef struct {
    fs_node_t *children[RAMFS_MAX_CHILDREN];
    int child_count;
    char *data;
} ramfs_node_t;


fs_node_t *ramfs_create_dir(const char *name);
fs_node_t *ramfs_create_file(const char *name, char *data);
void ramfs_add_child(fs_node_t *parent, fs_node_t *child);
#endif