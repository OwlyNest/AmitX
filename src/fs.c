
#include "fs.h"
#include "screen.h"
#include "string.h"

//#define NULL ((void*)0)

#define MAX_FILES 8

static File files[MAX_FILES];
static int file_count = 0;


void fs_init() {
    files[0].path = "/Saved/hello.txt";
    files[0].content = "Hello from /Saved/hello.txt!\nThis is a test file.";
    files[1].path = "/Saved/settings.cfg";
    files[1].content = "logo=big\ntheme=dark";
    file_count = 2;
    fs_add("/Saved/log.txt", "System log started.\n");
    fs_add("/Saved/me.txt", "Amity!");
}

const char* fs_read(const char* path) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].path, path) == 0) {
            return files[i].content;
        }
    }
    puts("fs_read: file not found: ");
    puts(path);
    puts("\n");
    return NULL;
}

void fs_debug_list() {
    for (int i = 0; i < file_count; i++) {
        puts("-> ");
        puts(files[i].path);
        newline();
    }
}

int fs_add(const char* path, const char* content) {
    if (file_count >= MAX_FILES) return 0;
    files[file_count].path = path;
    files[file_count].content = content;
    file_count++;
    return 1;
}
