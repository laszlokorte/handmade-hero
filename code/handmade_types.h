#if !defined(HANDMADE_TYPES_H)

#include <stdint.h>

#if HANDMADE_SLOW
#define Assert(Expression)                                                     \
  if (!(Expression)) {                                                         \
    *(int *)0 = 0;                                                             \
  }
#else
#define Assert(Expression)
#endif

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
#define NOTE_HALFTONE (1.0f / 12.0f)

#define Megabytes(m) (1024 * 1024 * ((uint64)m))
#define Gigabytes(m) (1024 * Megabytes(m))
#define Terabytes(m) (1024 * Gigabytes(m))


typedef size_t memory_index;

struct memory_arena {
  memory_index Size;
  memory_index Used;
  uint8 *Base;
};
internal void InitializeArena(memory_arena *Arena, memory_index Size,
                              uint8 *Base) {
  Arena->Size = Size;
  Arena->Used = 0;
  Arena->Base = Base;
}

#define ArenaPushStruct(Arena, Type) (Type *)ArenaPushSize(Arena, sizeof(Type))
#define ArenaPushArray(Arena, Type, Count)                                     \
  (Type *)ArenaPushSize(Arena, sizeof(Type) * Count)
internal void *ArenaPushSize(memory_arena *Arena, memory_index Size) {
  void *Result = Arena->Base + Arena->Used;
  Arena->Used += Size;

  return Result;
}

#define HANDMADE_TYPES_H
#endif
