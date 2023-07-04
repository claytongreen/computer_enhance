#ifndef ME_ARENA_H
#define ME_ARENA_H

#include <stdint.h>

typedef struct Arena Arena;
struct Arena {
  uint8_t *base;
  uint64_t offset;
  uint64_t commit;
  uint64_t capacity;
};

Arena *me_arena_create(void);
void   me_arena_destroy(Arena *arena);

void *me_arena_push(Arena *arena, uint64_t size);
void *me_arena_push_zero(Arena *arena, uint64_t size);
void  me_arena_pop(Arena *arena, uint64_t size);

#endif // ME_H

