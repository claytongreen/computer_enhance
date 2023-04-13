#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>

#define assert(Cond)                                                           \
  do {                                                                         \
    if (!(Cond))                                                               \
      __debugbreak();                                                          \
  } while (0)


typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;

typedef int32_t b32;

#endif // _TYPES_H
