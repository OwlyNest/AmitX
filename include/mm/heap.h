#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

void* malloc(size_t size);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t new_size);
void free(void* ptr);

void *memset(void *dest, int val, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
int memcmp(const void* s1, const void* s2, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void *memchr(const void *str, int c, size_t n);

void* sbrk(ptrdiff_t increment);
void print_heap_state();
#endif