#include <fs/vfs.h>
#include <stddef.h>

static fs_node_t *vfs_root = NULL;

void vfs_mount(fs_node_t *root) {
    vfs_root = root;
}

fs_node_t *vfs_resolve(const char *path) {
    if (!vfs_root || !path || path[0] != '/')
        return NULL;

    fs_node_t *current = vfs_root;
    char name[32];
    const char *p = path + 1;

    while (*p) {
        int i = 0;
        while (*p && *p != '/' && i < 31)
            name[i++] = *p++;
        name[i] = 0;

        if (*p == '/') p++;

        if (!(current->flags & FS_DIR) ||
            !current->ops || !current->ops->lookup)
            return NULL;

        current = current->ops->lookup(current, name);
        if (!current) return NULL;
    }

    return current;
}
