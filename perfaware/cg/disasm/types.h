#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>
#include <stdio.h>
#include <intrin.h> // malloc
#include <string.h> // memset

#define assert(Cond)                                                           \
  do {                                                                         \
    if (!(Cond))                                                               \
      __debugbreak();                                                          \
  } while (0)


#define ARRAY_COUNT(x) sizeof(x) / sizeof(x[0])

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

typedef int32_t b32;


#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)
#define GB(x) ((x) << 30)
#define TB(x) ((x) << 40)


#endif // _TYPES_H
