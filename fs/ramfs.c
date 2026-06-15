#include "fs/ramfs.h"
#include "lib/string.h"
static fs_node_t *ramfs_lookup(fs_node_t *dir, const char *name);
static int ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, char *buffer);
static int ramfs_write(fs_node_t *node, uint32_t offset, uint32_t size, const char *buffer);

static fs_ops_t ramfs_dir_ops = {
    .read   = NULL,
    .write  = NULL,
    .open   = NULL,
    .close  = NULL,
    .lookup = ramfs_lookup
};

static fs_ops_t ramfs_file_ops = {
    .read   = ramfs_read,
    .write  = ramfs_write,
    .open   = NULL,
    .close  = NULL,
    .lookup = NULL
};

fs_node_t *ramfs_create_dir(const char *name) {
    static fs_node_t nodes[32];
    static ramfs_node_t impls[32];
    static int idx = 0;

    fs_node_t *n = &nodes[idx];
    ramfs_node_t *r = &impls[idx];
    idx++;

    memset(n, 0, sizeof(*n));
    memset(r, 0, sizeof(*r));

    strncpy(n->name, name, 31);
    n->flags = FS_DIR;
    n->ops   = &ramfs_dir_ops;
    n->impl  = r;

    return n;
}

fs_node_t *ramfs_create_file(const char *name, char *data) {
    static fs_node_t nodes[32];
    static ramfs_node_t impls[32];
    static int idx = 0;

    fs_node_t *n = &nodes[idx];
    ramfs_node_t *r = &impls[idx];
    idx++;

    memset(n, 0, sizeof(*n));
    memset(r, 0, sizeof(*r));

    strncpy(n->name, name, 31);
    n->flags = FS_FILE;
    n->ops   = &ramfs_file_ops;
    n->impl  = r;

    r->data = data;
    n->size = strlen(data);

    return n;
}

void ramfs_add_child(fs_node_t *parent, fs_node_t *child) {
    ramfs_node_t *r = parent->impl;
    if (r->child_count >= RAMFS_MAX_CHILDREN)
        return;
    r->children[r->child_count++] = child;
}

static fs_node_t *ramfs_lookup(fs_node_t *dir, const char *name) {
    ramfs_node_t *r = dir->impl;
    for (int i = 0; i < r->child_count; i++) {
        if (!strcmp(r->children[i]->name, name))
            return r->children[i];
    }
    return NULL;
}

static int ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, char *buffer) {
    ramfs_node_t *r = node->impl;
    if (offset >= node->size)
        return 0;

    uint32_t to_read = size;
    if (offset + size > node->size)
        to_read = node->size - offset;

    memcpy(buffer, r->data + offset, to_read);
    return to_read;
}

static int ramfs_write(fs_node_t *node, uint32_t offset, uint32_t size, const char *buffer) {
    ramfs_node_t *r = node->impl;
    
    // Calculate required size
    uint32_t required_size = offset + size;
    
    // If we need more space, allocate a new buffer
    if (required_size > node->size) {
        char *new_data = (char *)malloc(required_size + 1);
        if (!new_data) return -1;
        
        // Copy old data if it exists
        if (r->data && node->size > 0) {
            memcpy(new_data, r->data, node->size);
        }
        free(r->data);
        
        // Zero out the gap if writing past current end
        if (offset > node->size) {
            memset(new_data + node->size, 0, offset - node->size);
        }
        
        r->data = new_data;
        node->size = required_size;
    }
    
    // Write the data
    memcpy(r->data + offset, buffer, size);
    
    // Null-terminate if this looks like a string operation
    if (r->data && required_size > 0) {
        r->data[required_size] = '\0';
    }
    
    return size;
}
