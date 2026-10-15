#ifndef BG_STDINT_H
#define BG_STDINT_H
#ifndef __int8_t_defined                /* as TinyCC's stddef.h has them */
#define __int8_t_defined
typedef signed char int8_t;
typedef signed short int int16_t;
typedef signed int int32_t;
#if defined(__LP64__) && !defined(__APPLE__)
typedef signed long int int64_t;
#else
typedef signed long long int int64_t;
#endif
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
#if defined(__LP64__) && !defined(__APPLE__)
typedef unsigned long int uint64_t;
#else
typedef unsigned long long int uint64_t;
#endif
#endif
#ifndef __TINYC__                        /* TinyCC's stddef.h has these too */
typedef __PTRDIFF_TYPE__ intptr_t;
typedef __SIZE_TYPE__ uintptr_t;
#endif
typedef int8_t int_least8_t; typedef uint8_t uint_least8_t;
typedef int16_t int_least16_t; typedef uint16_t uint_least16_t;
typedef int32_t int_least32_t; typedef uint32_t uint_least32_t;
typedef int64_t int_least64_t; typedef uint64_t uint_least64_t;
typedef int8_t int_fast8_t; typedef uint8_t uint_fast8_t;
typedef int32_t int_fast16_t; typedef uint32_t uint_fast16_t;
typedef int32_t int_fast32_t; typedef uint32_t uint_fast32_t;
typedef int64_t int_fast64_t; typedef uint64_t uint_fast64_t;
typedef int64_t intmax_t; typedef uint64_t uintmax_t;
#define INT8_MIN (-128)
#define INT8_MAX 127
#define UINT8_MAX 255
#define INT16_MIN (-32768)
#define INT16_MAX 32767
#define UINT16_MAX 65535
#define INT32_MIN (-2147483647 - 1)
#define INT32_MAX 2147483647
#define UINT32_MAX 4294967295U
#define INT64_MAX 9223372036854775807LL
#define INT64_MIN (-INT64_MAX - 1LL)
#define UINT64_MAX 18446744073709551615ULL
#define INT64_C(x) x##LL
#define UINT64_C(x) x##ULL
#define INT32_C(x) x
#define UINT32_C(x) x##U
#endif
