#ifndef __MM_HEAP_H__
#define __MM_HEAP_H__

#include <stddef.h>
#include <stdint.h>

extern PUCHAR heap_base;
extern PUCHAR heap_end;
extern PUCHAR heap_break;

PVOID malloc(SIZE_T size);
PVOID calloc(SIZE_T num, SIZE_T size);
PVOID realloc(PVOID ptr, SIZE_T new_size);
VOID free(PVOID ptr);

PVOID sbrk(SSIZE_T increment);
VOID print_heap_state(VOID);

#define kmalloc malloc
#define kfree free
#define kcalloc calloc
#define krealloc realloc
#endif