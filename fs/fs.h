
#ifndef FS_H
#define FS_H

void fs_init();
const char* fs_read(const char* path);
int fs_write(const char* path, const char* content);
void fs_debug_list();
int fs_add(const char* path, const char* content);

#endif