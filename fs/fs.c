#include "vfs.h"
#include "ramfs.h"
#include "screen.h"
#include "string.h"

int fs_add(const char* path, const char* content);

void fs_init() {
    fs_node_t *root = ramfs_create_dir("");
    fs_node_t *saved = ramfs_create_dir("Saved");

    ramfs_add_child(root, saved);
    ramfs_add_child(saved, ramfs_create_file("hello.txt", "Hello from AmitX!\n"));
    ramfs_add_child(saved, ramfs_create_file("log.txt", "System log started\n"));

    vfs_mount(root);
}

const char* fs_read(const char* path) {
    fs_node_t *node = vfs_resolve(path);
    if (!node || !(node->flags & FS_FILE))
        return NULL;
    
    ramfs_node_t *impl = (ramfs_node_t*)node->impl;
    return impl->data;
}

int fs_write(const char* path, const char* content) {
    fs_node_t *node = vfs_resolve(path);
    
    // If file doesn't exist, create it first
    if (!node) {
        if (fs_add(path, content) != 0)
            return -1;
        // After creating, resolve it again to get the node
        node = vfs_resolve(path);
        if (!node)
            return -1;
    }
    
    if (!(node->flags & FS_FILE))
        return -1;
    
    if (!node->ops || !node->ops->write)
        return -1;
    
    return node->ops->write(node, 0, strlen(content), content);
}

int fs_add(const char* path, const char* content) {
    // Parse the path to find parent and filename
    char parent_path[64];
    char filename[32];
    
    const char *last_slash = strrchr(path, '/');
    if (!last_slash)
        return -1;
    
    // Extract parent path
    int parent_len = last_slash - path;
    if (parent_len == 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
    }
    
    // Extract filename
    strncpy(filename, last_slash + 1, 31);
    filename[31] = '\0';
    
    // Find parent directory
    fs_node_t *parent = vfs_resolve(parent_path);
    if (!parent || !(parent->flags & FS_DIR))
        return -1;
    
    // Create new file with allocated data
    char *data = (char *)malloc(strlen(content) + 1);
    if (!data)
        return -1;
    
    strcpy(data, content);
    fs_node_t *new_file = ramfs_create_file(filename, data);
    
    // Add to parent
    ramfs_add_child(parent, new_file);
    
    return 0;
}

void fs_debug_list(void) {
    puts("[fs_debug] Listing filesystem structure:\n");
    // Simple recursive listing would require more infrastructure
    // For now, just show that the function exists
}