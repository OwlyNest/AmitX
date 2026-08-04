/*
	* include/internal/phonon_types.h - [Enter description]
	* Author:   amity
	* Date:     Tue Aug  4 15:28:52 2026
	* Copyright © 2026 OwlyNest
*/

/* --- Styling Instructions ---
	* Encoding:                      UTF-8, Unix line endings
	* Text font:                     Monospace
	* Line width:                    Max 80 characters
	* Indentation:                   Use 4 spaces
	* Brace style:                   Same line as control statement
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/
#ifndef __INTERNAL_PHONON_TYPES_H__
#define __INTERNAL_PHONON_TYPES_H__

/* --- Includes ---*/
#include <stdint.h>
#include <stddef.h>

/* --- Typedefs - Structs - Enums ---*/

/*
 * Fundamental integer types
*/

typedef int8_t      CHAR;
typedef uint8_t     UCHAR;

typedef int16_t     SHORT;
typedef uint16_t    USHORT;

typedef int32_t     LONG;
typedef uint32_t    ULONG;

typedef int64_t     LONGLONG;
typedef uint64_t    ULONGLONG;

typedef uint8_t     BYTE;
typedef uint16_t    WORD;
typedef uint32_t    DWORD;
typedef uint64_t    QWORD;

/*
 * Boolean
*/

typedef uint8_t     BOOLEAN;


/*
 * Pointer-sized integers
*/

typedef uintptr_t   ULONG_PTR;
typedef intptr_t    LONG_PTR;

typedef ULONG_PTR   SIZE_T;
typedef LONG_PTR    SSIZE_T;

/*
 * Generic pointers
*/

typedef void        VOID;
typedef void       *PVOID;
typedef CHAR       *PCHAR;
typedef UCHAR      *PUCHAR;

typedef const void *PCVOID;


/*
 * Handles
*/

typedef PVOID       HANDLE;
typedef HANDLE     *PHANDLE;

/*
 * Status codes
*/

typedef LONG        SMSTATUS;


/*
 * Physical addresses
*/

typedef ULONGLONG   PHYSICAL_ADDRESS;
typedef PHYSICAL_ADDRESS *PPHYSICAL_ADDRESS;

/* --- Globals ---*/

/* --- Prototypes ---*/

#endif