#ifndef __ACAMITX_H__
#define __ACAMITX_H__

#define ACPI_MACHINE_WIDTH          32
#define ACPI_USE_LOCAL_CACHE
#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_NATIVE_DIVIDE
#define ACPI_MUTEX_TYPE ACPI_OSL_MUTEX
#define ACPI_DEBUG_OUTPUT

#include <stdint.h>
#include <stddef.h>
#include <lib/string.h>

#ifndef ACPI_USE_NATIVE_DIVIDE

#ifndef ACPI_DIV_64_BY_32
#define ACPI_DIV_64_BY_32(n_hi, n_lo, d32, q32, r32) \
do { \
    UINT64 __n = ((UINT64)(n_hi) << 32) | (UINT32)(n_lo); \
    (q32) = (UINT32)(__n / (d32)); \
    (r32) = (UINT32)(__n % (d32)); \
} while (0)
#endif

#ifndef ACPI_SHIFT_RIGHT_64
#define ACPI_SHIFT_RIGHT_64(n_hi, n_lo) \
do { \
    (n_lo) >>= 1; \
    (n_lo) |= (((n_hi) & 1) << 31); \
    (n_hi) >>= 1; \
} while (0)
#endif

#endif

#endif /* __ACAMITX_H__ */
