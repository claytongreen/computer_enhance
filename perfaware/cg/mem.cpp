#include "mem.h"

#define DEFAULT_ARENA_SIZE MB(4)

arena_t *arena_create(void) {
  arena_t *result = 0;

  u8 *data = (u8 *)malloc(sizeof(arena_t) + DEFAULT_ARENA_SIZE);

  result = (arena_t *)data;
  // TODO: pow2 align
  result->pos = sizeof(*result);
  result->cap = DEFAULT_ARENA_SIZE;

  return result;
}

void arena_reset(arena_t *arena) {
  arena->pos = sizeof(*arena);
}

void *arena_push(arena_t *arena, u64 size) {
  void *result = 0;

  assert(arena->pos + size <= arena->cap && "Arena out of memory");

  result = (u8 *)arena + arena->pos;
  arena->pos += size;

  assert(result != 0 && "Arena failed to allocate");

  return result;
}

void *arena_push_zero(arena_t *arena, u64 size) {
  void *result = arena_push(arena, size);
  memset(result, 0, size);
  return result;
}

void arena_pop_to(arena_t *arena, u64 pos) {
  arena->pos = pos;
  // TODO: handle decommit
}

arena_temp_t arena_temp_begin(arena_t *arena) {
  arena_temp_t result = { 0 };
  result.arena = arena;
  result.pos = arena->pos;
  return result;
}

void arena_temp_end(arena_temp_t temp) {
  arena_pop_to(temp.arena, temp.pos);
}

