#ifndef __MM_HEAP_H__
#define __MM_HEAP_H__

#include <stddef.h>
#include <stdint.h>

extern uint8_t* heap_base;
extern uint8_t* heap_end;
extern uint8_t* heap_break;

void* malloc(size_t size);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t new_size);
void free(void* ptr);

void* sbrk(ptrdiff_t increment);
void print_heap_state();
#endif