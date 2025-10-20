
#ifndef FS_H
#define FS_H

typedef struct {
    const char* path;
    const char* content;
} File;


void fs_init();
const char* fs_read(const char* path);
void fs_debug_list();
int fs_add(const char* path, const char* content);

#endif
