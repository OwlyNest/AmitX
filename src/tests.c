#include "string.h"
#include "screen.h"
#include "heap.h"
#include "keyboard.h"
#include "kernel.h"
#include "fs.h"
#include "time.h"

extern int load_cyclone;

void test_0div() {
    volatile int one = 1;
    volatile int zero = 0;
    volatile int c = one / zero;
    (void) c;
}

void test_fs() {
    fs_init();
    fs_debug_list();

    const char* content = fs_read("/Saved/hello.txt");
    if (content) {
        puts("\nContent of hello.txt:\n");
        puts(content);
    } else {
        puts("File not found.\n");
    }
    newline();
    const char* content2 = fs_read("/Saved/log.txt");
    if (content2) {
        puts("\nContent of log.txt:\n");
        puts(content2);
    } else {
        puts("File not found.\n");
    }
    newline();
    const char* content3 = fs_read("/Saved/me.txt");
    if (content3) {
        puts("\nContent of me.txt:\n");
        puts(content3);
    } else {
        puts("File not found.\n");
    }
}

void test_heap() {
    puts("[heap] Running heap tests...\n");

    char* a = (char*)malloc(16);
    char* b = (char*)malloc(32);

    if (a && b) {
        strcpy(a, "Hello");
        strcpy(b, "World");

        puts("[heap] a = ");
        
        puts(a);
        puts("\n");

        puts("[heap] b = ");
        puts(b);
        puts("\n");
    } else {
        puts("[heap] Allocation failed!\n");
    }

    // Allocate more to push boundaries
    void* c = malloc(0xFF00); // almost full heap
    if (c) {
        puts("[heap] Large allocation succeeded\n");
    } else {
        puts("[heap] Large allocation failed\n");
    }

    // Check exhaustion
    void* d = malloc(1024); // should fail now
    if (d) {
        puts("[heap] Unexpectedly succeeded after exhaustion\n");
    } else {
        puts("[heap] Correctly failed after exhaustion\n");
    }
}

void test_strdup() {
    const char* original = "CycloneSpin";
    char* copy = strdup(original);

    if (!copy) {
        puts("[strdup] strdup returned NULL!\n");
        return;
    }

    puts("[strdup] Original: ");
    puts(original);
    puts("\n");

    puts("[strdup] Copy    : ");
    puts(copy);
    puts("\n");

    // Modify copy to verify it's independent
    copy[0] = 'X';

    puts("[strdup] Modified Copy: ");
    puts(copy);
    puts("\n");

    puts("[strdup] Original should still be: ");
    puts(original);
    puts("\n");
}

void test_malloc_free() {
    char* a = (char*)malloc(12);
    strcpy(a, "Hello");
    puts("[heap] A: ");
    puts(a);
    newline();

    free(a);

    char* b = (char*)malloc(8);
    strcpy(b, "World");
    puts("[heap] B: ");
    puts(b);
    newline();

    // Ideally, b == a (reuse check)
    if (a == b) {
        puts("[heap] Reused freed block\n");
    } else {
        puts("[heap] Allocated new block\n");
    }
}

void test_calloc() {

    char* data = (char*)calloc(10, sizeof(char));

    if (!data) {
        puts("[heap] calloc failed.\n");
        return;
    }

    // Check if all bytes are 0
    int success = 1;
    for (int i = 0; i < 10; i++) {
        if (data[i] != 0) {
            success = 0;
            break;
        }
    }

    if (success) {
        puts("[heap] calloc returned zero-initialized memory\n");
    } else {
        puts("[heap] calloc memory not zeroed\n");
    }

    free(data); // Optional, to test free reuse too
}

void string_and_heap_test() {

    // === strdup / strdup_n ===
    char* a = strdup("hello world");
    char* b = strdup_n("goodbye world", 7);
    puts("[strdup] A = "); puts(a); putc('\n');
    puts("[strdup_n] B = "); puts(b); putc('\n');

    // === strcmp / strncmp ===
    puts("[strcmp] ");
    putint(strcmp("abc", "abc")); putc('\n');
    puts("[strncmp] ");
    putint(strncmp("abcdef", "abcxyz", 3)); putc('\n');

    // === strcpy / strncpy ===
    char dest1[16], dest2[16];
    strcpy(dest1, "fast copy");
    strncpy(dest2, "slow copy", 4);
    dest2[4] = '\0';
    puts("[strcpy] "); puts(dest1); putc('\n');
    puts("[strncpy] "); puts(dest2); putc('\n');

    // === strcat ===
    char catbuf[32] = "hello ";
    strcat(catbuf, "there");
    puts("[strcat] "); puts(catbuf); putc('\n');

    // === strnlen ===
    puts("[strnlen] ");
    putint(strnlen("lengthy string", 7)); putc('\n');

    // === strchr / strchrnul / strrchr ===
    const char* test = "abcabcabcz";
    puts("[strchr] "); putc(*strchr(test, 'b')); putc('\n');
    puts("[strchrnul] ");
    char es = *strchrnul(test, 'z');
    putc(es);
    putc('\n');
    puts("[strrchr] "); putc(*strrchr(test, 'b')); putc('\n');

    // === memset / memcpy / memmove / memcmp ===
    char buf1[10], buf2[10];
    memset(buf1, 'X', 5);
    buf1[5] = '\0';
    puts("[memset] "); puts(buf1); putc('\n');

    memcpy(buf2, "abcde", 6);
    puts("[memcpy] "); puts(buf2); putc('\n');

    memmove(buf2 + 2, buf2, 4);  // Overlap test
    buf2[6] = '\0';
    puts("[memmove] "); puts(buf2); putc('\n');

    puts("[memcmp] ");
    putint(memcmp("aaa", "aab", 3)); putc('\n');

    // === malloc / calloc / free ===
    char* m = malloc(10);
    char* c = calloc(5, 2);
    strcpy(m, "malloc");
    puts("[malloc] "); puts(m); putc('\n');

    puts("[calloc] ");
    for (int i = 0; i < 10; i++) {
        putint(c[i]); putc(' ');
    }
    putc('\n');

    free(m);
    free(c);
}

void test(int testnum) {
    clear();
    puts("Press 'q' to return to main menu\n");

    switch (testnum) {
        case 0:
            puts("[test]: 0-division test\n");
            test_0div();
            break;
        case 1:
            puts("[test]: VFS test\n");
            test_fs();
            break;
        case 2:
            puts("[test]: heap test\n");
            test_heap();
            break;
        case 3:
            puts("[test]: strdup test\n");
            test_strdup();
            break;
        case 4:
            puts("[test]: malloc and free test\n");
            test_malloc_free();
            break;
        case 5:
            puts("[test]: calloc test\n");
            test_calloc();
            break;
        case 6:
            puts("[test]: string and heap test\n");
            string_and_heap_test();
            break;
        default:
            setcolor(0,15);
            puts("test not found\n");
            setcolor(15,0);
            break;
    }
    while (1) {
        char c = keyboard_getchar();
        if (c == 'q') {
            load_cyclone = 0;
            clear();
            draw_start();
        }
    }
}