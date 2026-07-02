/*
	* include/lib/math.h - [Enter description]
	* Author:   amity
	* Date:     Thu Jul  2 14:53:00 2026
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
#ifndef MATH_H
#define MATH_H

#define CORDIC_ITERATIONS   16
#define CORDIC_K_INV        107829

#define FX_SHIFT            16
#define FX_ONE              (1 << FX_SHIFT)
#define FX_MASK             (FX_ONE - 1)
#define FX_FROM_INT(i)      ((int32_t)(i) << FX_SHIFT)
#define FX_TO_INT(f)        ((int32_t)((f) >> FX_SHIFT))
#define FX_TO_INT_RND(f)    ((int32_t)(((f) + (FX_ONE >> 1)) >> FX_SHIFT))
#define FX_MUL(a, b)        ((int32_t)(((int64_t)(a) * (int64_t)(b)) >> FX_SHIFT))
#define FX_DIV(a, b)        ((int32_t)((((int64_t)(a) << FX_SHIFT) / (int64_t)(b))))
#define ANGLE_FULL_TENTHS   3600     /* tenths of degrees */
#define ANGLE_FULL_BRAD     65536    /* bradians: 0x10000 = 360° */
#define BRAD_90             0x4000   /* 16384 */
#define BRAD_180            0x8000   /* 32768 */
#define BRAD_270            0xC000   /* 49152 */

/* --- Includes ---*/
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/
typedef int32_t fx_t;

typedef struct {
    int x;
    int y;
} vec2i_t;

typedef struct {
    fx_t x;
    fx_t y;
} vec2f_t;

/* --- Globals ---*/

/* --- Prototypes ---*/
/* Fixed-point basics */
fx_t fx_from_int(int32_t i);
int32_t fx_to_int(fx_t f);
int32_t fx_to_int_rnd(fx_t f);
fx_t fx_mul(fx_t a, fx_t b);
fx_t fx_div(fx_t a, fx_t b);
fx_t fx_sqrt(fx_t x);

/* CORDIC trig: angle in bradians (0-65535, 0x10000 = 360°) */
fx_t fx_sin_b(int angle_bradians);
fx_t fx_cos_b(int angle_bradians);

/* Angle conversion */
int angle_to_bradians(int tenths_of_degrees);

/* Vector helpers */
vec2i_t vec2i(int x, int y);
vec2i_t vec2i_add(vec2i_t a, vec2i_t b);
vec2i_t vec2i_sub(vec2i_t a, vec2i_t b);
vec2i_t vec2i_polar_bradians(int cx, int cy, int radius, int angle_bradians);

/* Integer distance */
int32_t idist(int x0, int y0, int x1, int y1);
int32_t idist_sq(int x0, int y0, int x1, int y1);

/* Clamp */
int32_t iclamp(int32_t v, int32_t lo, int32_t hi);
uint32_t uclamp(uint32_t v, uint32_t lo, uint32_t hi);
#endif