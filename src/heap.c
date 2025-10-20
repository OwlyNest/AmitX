
#include "heap.h"
#include "screen.h"  // for test/debug prints
#include <stdint.h>
#include <stddef.h>
#include "string.h"

#define HEAP_START 0x100000
#define HEAP_SIZE  0x10000
#define ALIGN16(x) (((x) + 15) & ~15)

typedef struct Block {
    size_t size;
    struct Block* next;
    int free;
} __attribute__((aligned(16))) Block;

#define BLOCK_SIZE sizeof(Block)

static uint8_t* heap_base = (uint8_t*)HEAP_START;
static uint8_t* heap_end  = (uint8_t*)(HEAP_START + HEAP_SIZE);

static Block* head = NULL;

void* malloc(size_t size) {
    if (size == 0) return NULL;

    size = ALIGN16(size);

    if (!head) {
        head = (Block*)heap_base;
        head->size = size;
        head->next = NULL;
        head->free = 0;
        return (void*)(head + 1);
    }

    Block* current = head;
    while (current) {
        if (current->free && current->size >= size) {
            // Check if we can split the block
            size_t leftover = current->size - size;
            if (leftover > BLOCK_SIZE + 8) { // 8 = minimal leftover size to make a new block useful
                // Split block
                Block* new_block = (Block*)((uint8_t*)(current + 1) + size);
                new_block->size = leftover - BLOCK_SIZE;
                new_block->free = 1;
                new_block->next = current->next;

                current->size = size;
                current->free = 0;
                current->next = new_block;

                return (void*)(current + 1);
            } else {
                // Can't split, allocate entire block
                current->free = 0;
                return (void*)(current + 1);
            }
        }

        if (!current->next) break;
        current = current->next;
    }

    // No suitable free block found, append new block at the end (same as before)
    uint8_t* next_addr = (uint8_t*)current + BLOCK_SIZE + current->size;
    uint8_t* alloc = sbrk(BLOCK_SIZE + size);
    if (alloc == (void*)-1 || alloc != next_addr) {
        return NULL;
    }

    Block* new_block = (Block*)next_addr;
    new_block->size = size;
    new_block->free = 0;
    new_block->next = NULL;
    current->next = new_block;

    return (void*)(new_block + 1);
}

void free(void* ptr) {
    if (!ptr) return;
    Block* block = (Block*)ptr - 1;
    if (block->free) return;
    block->free = 1;

    Block* next = block->next;
    if (next && next->free) {
        block->size += BLOCK_SIZE + next->size;
        block->next = next->next;
    }
}

void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *memset(void *dest, int val, size_t len) {
    unsigned char *ptr = dest;
    while (len-- > 0)
        *ptr++ = (unsigned char)val;
    return dest;
}

void *memcpy(void *dest, const void *src, size_t len) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (len--) 
        *d++ = *s++;
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* a = s1;
    const unsigned char* b = s2;

    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return a[i] - b[i];
        }
    }
    return 0;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;

    if (d == s) {
        return dest;
    }

    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

void* realloc(void* ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    Block* block = (Block*)ptr - 1;
    if (block->size >= new_size) {
        return ptr;
    }

    void* new_ptr = malloc(new_size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}

void print_block(Block* b) {
    puts("Block @ ");
    puthex((uint32_t)b);
    puts(", size=");
    char buf[12];
    int_to_ascii((int)b->size, buf);
    puts(buf);
    puts(", free=");
    puts(b->free ? "yes" : "no");
    puts("\n");
}

void print_heap_state() {
    Block* current = head;
    puts("Heap blocks:\n");
    while (current) {
        print_block(current);
        current = current->next;
    }
}
void test_malloc_splitting(void) {
    puts("[Test] malloc + splitting\n");

    // Allocate a big block
    void* p1 = malloc(100);
    puts("Allocated p1 = ");
    puthex((uint32_t)p1);
    puts("\n");

    // Allocate another smaller block, should create new block
    void* p2 = malloc(50);
    puts("Allocated p2 = ");
    puthex((uint32_t)p2);
    puts("\n");

    print_heap_state();

    // Free p2 so the block is free now
    free(p2);
    puts("Freed p2\n");

    print_heap_state();

    // Allocate smaller block than p2's size (should split)
    void* p3 = malloc(20);
    puts("Allocated p3 (should split free block) = ");
    puthex((uint32_t)p3);
    puts("\n");

    print_heap_state();
}

void test_realloc() {

    char* p1 = malloc(10);
    for (int i = 0; i < 10; i++) {
        p1[i] = 'A' + i;
    }

    puts("Allocated p1 = ");
    puthex((uint32_t)p1);
    puts("\n");

    char* p2 = realloc(p1, 20);
    puts("Reallocated p1 to p2 = ");
    puthex((uint32_t)p2);
    puts("\n");

    puts("p2 contents: ");
    for (int i = 0; i < 10; i++) {
        putc(p2[i]);
    }
    puts("\n");

    print_heap_state();
}

static uint8_t* heap_break = (uint8_t*)HEAP_START;

void* sbrk(ptrdiff_t increment) {
    uint8_t* prev_break = heap_break;
    uint8_t* new_break = heap_break + increment;

    if (new_break > heap_end || new_break < (uint8_t*)HEAP_START) {
        return (void*)-1; // error
    }

    heap_break = new_break;
    return prev_break;
}

void test_sbrk() {
    puts("[Test] sbrk\n");

    void* a = sbrk(0);
    puts("Heap start: "); puthex((uint32_t)a); puts("\n");

    void* b = sbrk(128);
    puts("Allocated 128 bytes via sbrk: "); puthex((uint32_t)b); puts("\n");

    void* c = sbrk(0);
    puts("New break: "); puthex((uint32_t)c); puts("\n");
}

void test_heap_final() {
    puts("[heap test] Start\n");

    char* a = malloc(64);
    char* b = malloc(32);
    char* c = malloc(16);
    print_heap_state();

    free(b);
    print_heap_state();

    char* d = malloc(24); // should reuse b's slot and split
    print_heap_state();

    free(a);
    free(c);
    free(d);
    print_heap_state();

    puts("[heap test] Done\n");
}
