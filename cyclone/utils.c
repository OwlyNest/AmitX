#include "utils.h"
#include "string.h"

int starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*prefix++ != *str++) {
            return 0;
        }
    }
    return 1;
}
