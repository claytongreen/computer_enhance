#ifndef _MEM_H
#define _MEM_H

#include "types.h"

#define PUSH_ARRAY(TYPE, COUNT)                                                \
  (TYPE *)global_memory;                                                       \
  assert((sizeof(TYPE) * (COUNT)) < global_memory_size &&                      \
         "ALLOC SIZE TOO LARGE");                                              \
  memset(global_memory, 0, sizeof(TYPE) * (COUNT));                            \
  global_memory += sizeof(TYPE) * (COUNT);                                     \
  assert(                                                                      \
      ((size_t)(global_memory - global_memory_base) < global_memory_size) &&   \
      "OUT OF MEMORY")

static size_t global_memory_size = 1024 * 1024 * 8;
static u8 *global_memory_base;
static u8 *global_memory;

#endif // _MEM_H
