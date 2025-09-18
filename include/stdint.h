#ifndef _KFS_STDINT_H
#define _KFS_STDINT_H

/* Minimal stdint.h for 32-bit i386 freestanding kernel */

/* Exact-width integer types */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

/* Minimum-width integer types */
typedef int8_t int_least8_t;
typedef uint8_t uint_least8_t;
typedef int16_t int_least16_t;
typedef uint16_t uint_least16_t;
typedef int32_t int_least32_t;
typedef uint32_t uint_least32_t;
typedef int64_t int_least64_t;
typedef uint64_t uint_least64_t;

/* Fastest minimum-width (for simplicity map to native) */
typedef int8_t int_fast8_t;
typedef uint8_t uint_fast8_t;
typedef int32_t int_fast16_t; /* i386: int likely fastest */
typedef uint32_t uint_fast16_t;
typedef int32_t int_fast32_t;
typedef uint32_t uint_fast32_t;
typedef int64_t int_fast64_t;
typedef uint64_t uint_fast64_t;

/* Pointer-related */
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;

/* Greatest-width */
typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

/* Limits */
#define INT8_MIN (-128)
#define INT8_MAX 127
#define UINT8_MAX 255
#define INT16_MIN (-32768)
#define INT16_MAX 32767
#define UINT16_MAX 65535
#define INT32_MIN (-2147483647 - 1)
#define INT32_MAX 2147483647
#define UINT32_MAX 4294967295U
#define INT64_MIN (-9223372036854775807LL - 1)
#define INT64_MAX 9223372036854775807LL
#define UINT64_MAX 18446744073709551615ULL

#define INTMAX_MIN INT64_MIN
#define INTMAX_MAX INT64_MAX
#define UINTMAX_MAX UINT64_MAX

#define INTPTR_MIN INT32_MIN
#define INTPTR_MAX INT32_MAX
#define UINTPTR_MAX UINT32_MAX

/* Other commonly used macros */
#define SIZE_MAX UINT32_MAX

#endif /* _KFS_STDINT_H */
