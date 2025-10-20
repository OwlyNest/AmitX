
#include "string.h"
#include <stdint.h>
#include "heap.h"

void int_to_ascii(int n, char str[]) {
    int i = 0, sign = n;
    if (n < 0) n = -n;

    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0)
        str[i++] = '-';

    str[i] = '\0';

    // Reverse
    for (int j = 0; j < i / 2; j++) {
        char tmp = str[j];
        str[j] = str[i - 1 - j];
        str[i - 1 - j] = tmp;
    }
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

uint32_t atoi(const char* str) {
    uint32_t result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

char* strcat(char* dest, const char* src) {
    char* original = dest;

    while (*dest) {
        dest++;
    }

    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';
    return original;
}

char* strcpy(char* dest, const char* src) {
    char* original = dest;
    while ((*dest++ = *src++));
    return original;
}

char* strchr(const char* str, int c) {
    while (*str) {
        if (*str == (char)c) {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}

char* strdup(const char* str) {
    size_t len = strlen(str);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* original = dest;
    while (n && *src) {
        *dest++ = *src++;
        n--;
    }
    while (n--) {
        *dest++ = '\0';
    }
    return original;
}

size_t strnlen(const char* s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len]) {
        len++;
    }
    return len;
}

char* strchrnul(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }
    return (char*)s;  // Return pointer to null terminator
}

char* strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    return (char*)last;
}

char* strdup_n(const char* s, size_t n) {
    size_t len = strnlen(s, n);
    char* copy = (char*)malloc(len + 1);
    if (!copy) return NULL;
    for (size_t i = 0; i < len; i++) {
        copy[i] = s[i];
    }
    copy[len] = '\0';
    return copy;
}
