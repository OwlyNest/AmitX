#ifndef VFS_H
#define VFS_H

#include <stdint.h>

#define FS_FILE 0x1
#define FS_DIR  0x2

typedef struct fs_node fs_node_t;
typedef struct fs_ops fs_ops_t;

struct fs_ops {
    int (*read)(fs_node_t*, uint32_t offset, uint32_t size, char *buffer);
    int (*write)(fs_node_t*, uint32_t offset, uint32_t size, const char *buffer);
    int (*open)(fs_node_t*);
    int (*close)(fs_node_t*);
    fs_node_t *(*lookup)(fs_node_t*, const char *name);
};

struct fs_node {
    char name[32];
    uint32_t size;
    uint32_t flags;
    void *impl;       // FS-specific data
    fs_ops_t *ops;
};
void vfs_mount(fs_node_t *root);
fs_node_t *vfs_resolve(const char *path);

#endif