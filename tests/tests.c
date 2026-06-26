#include <lib/string.h>
#include <tests/pmm_test.h>
#include <screen/screen.h>
#include <mm/heap.h>
#include <drivers/keyboard.h>
#include <kernel/kernel.h>
#include <screen/printk.h>
#include <tests/tests.h>

extern int load_cyclone;
extern int menu;

void test_0div() {
    volatile int one = 1;
    volatile int zero = 0;
    volatile int c = one / zero;
    (void) c;
}

void test_printk() {
    printk("\n[printk] s, c, %%:\n\n");
    printk("Hello %s\n", "world");
    printk("Char: %c\n", 'A');
    printk("100%% done\n");

    printk("\n[printk] u, x, X\n\n");
    printk("%u\n", 0);
    printk("%u\n", 12345);

    printk("%x\n", 0);
    printk("%x\n", 0xABCD);

    printk("%X\n", 0xABCD);

    printk("\n[printk] formatted x\n\n");
    printk("[%8x]\n", 0xAB);
    printk("[%08x]\n", 0xAB);
    printk("[%2x]\n", 0xABCD);
    printk("[%0x]\n", 0xAB);

    printk("\n[printk] formatted d\n\n");
    printk("%d\n", -42);
    printk("%5d\n", -42);
    printk("%05d\n", -42);
}

void test_fs() {
    puts("Under construction");
}

void test_heap() {
    printk("[heap] Running heap tests...\n");

    char* a = (char*)malloc(16);
    char* b = (char*)malloc(32);

    if (a && b) {
        strcpy(a, "Hello");
        strcpy(b, "World");

        printk("[heap] a = %s\n", a);

        printk("[heap] b = %s\n", b);
    } else {
        printk("[heap] Allocation failed!\n");
    }

    void* c = malloc(0xFF00);
    if (c) {
        printk("[heap] Large allocation succeeded\n");
    } else {
        printk("[heap] Large allocation failed\n");
    }

    // Check exhaustion
    void* d = malloc(1024);
    if (d) {
        printk("[heap] Unexpectedly succeeded after exhaustion\n");
    } else {
        printk("[heap] Correctly failed after exhaustion\n");
    }
}

void test_strdup() {
    const char* original = "CycloneSpin";
    char* copy = strdup(original);

    if (!copy) {
        printk("[strdup] strdup returned NULL!\n");
        return;
    }

    printk("[strdup] Original: %s\n", original);

    printk("[strdup] Copy: %s\n", copy);

    copy[0] = 'X';

    printk("[strdup] Modified Copy: %s\n", copy);

    printk("[strdup] Original should still be: %s\n", original);
}

void test_malloc_free() {
    char* a = (char*)malloc(12);
    strcpy(a, "Hello");
    printk("[heap] A: %s\n", a);

    free(a);

    char* b = (char*)malloc(8);
    strcpy(b, "World");
    printk("[heap] B: %s\n", b);

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

    free(data);
}

void string_and_heap_test() {

    // === strdup / strdup_n ===
    char* a = strdup("hello world");
    char* b = strdup_n("goodbye world", 7);
    printk("[strdup] A = %s\n", a);
    printk("[strdup] b = %s\n", b);

    // === strcmp / strncmp ===
    printk("[strcmp] %d\n", strcmp("abc", "abc"));
    printk("[strncmp] %d\n", strncmp("abcdef", "abcxyz", 3));

    // === strcpy / strncpy ===
    char dest1[16], dest2[16];
    strcpy(dest1, "fast copy");
    strncpy(dest2, "slow copy", 4);
    dest2[4] = '\0';
    printk("[strcpy] %s\n", dest1);
    printk("[strncpy] %s\n", dest2);

    // === strcat ===
    char catbuf[32] = "hello ";
    strcat(catbuf, "there");
    printk("[strcat] %s\n", catbuf);

    printk("[strnlen] %d\n", strnlen("lengthy string", 7));

    const char* test = "abcabcabcz";
    printk("[strchr] %c\n", *strchr(test, 'b'));
    char es = *strchrnul(test, 'z');
    printk("[strchrnul] %c\n", es);
    printk("[strrchr] %c\n", *strrchr(test, 'b'));

    char buf1[10], buf2[10];
    memset(buf1, 'X', 5);
    buf1[5] = '\0';

    printk("[memset] %s\n", buf1);

    memcpy(buf2, "abcde", 6);
    printk("[memcpy] %s\n", buf2);

    memmove(buf2 + 2, buf2, 4);
    buf2[6] = '\0';
    printk("[memmove] %s\n", buf2);
    
    printk("[memcmp] %d\n", memcmp("aaa", "aab", 3));

    char* m = malloc(10);
    char* c = calloc(5, 2);
    strcpy(m, "malloc");
    printk("[malloc] %s\n", m);

    printk("[calloc] ");
    for (int i = 0; i < 10; i++) {
        printk("%d ",c[i]);
    }
    printk("\n");

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
        case 7:
            puts("[test]: printk test\n");
            test_printk();
            break;
        case 8:
            puts("[test]: PMM test");
            pmm_run_tests();
            break;
        default:
            setcolor(0,15);
            puts("test not found\n");
            setcolor(15,0);
            break;
    }
    clear();
}