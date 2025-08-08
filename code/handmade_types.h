#if !defined (HANDMADE_TYPES_H)

#include <stdint.h>

#define local_persist static
#define global_variable static
#define internal static

typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef int64_t int64;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;

typedef float real32;
typedef double real64;

#define Pi32 3.14159265359f

#define HANDMADE_TYPES_H
#endif
