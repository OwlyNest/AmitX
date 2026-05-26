
#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

void int_to_ascii(int n, char str[]);
int strcmp(const char* s1, const char* s2);
uint32_t atoi(const char* str);
char* strcat(char* dest, const char* src);
char* strcpy(char* dest, const char* src);
char* strchr(const char* srt, int c);
char* strdup(const char* str);
size_t strlen(const char* str);
int strncmp(const char* s1, const char* s2, size_t n);
char* strncpy(char* dest, const char* src, size_t n);
size_t strnlen(const char* s, size_t maxlen);
char* strchrnul(const char* s, int c);
char* strrchr(const char* s, int c);
char* strdup_n(const char* s, size_t n);

#endif
