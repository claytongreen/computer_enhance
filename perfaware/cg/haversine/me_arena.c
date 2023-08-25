#include "me_arena.h"
#include "os.h"

static uint64_t align_up(uint64_t x) {
  uint64_t result = x + (~x & 63);
  return result;
}

static uint64_t align_down(uint64_t x) {
  uint64_t result = x & -64;
  return result;
}

Arena *me_arena_create(void) {
  Arena *result = 0;

  uint64_t capacity = GB((uint64_t)64);

  uint8_t *base = (uint8_t *)os_memory_reserve(capacity);
  if (base) {
    uint64_t page_size = os_page_size();
    // TODO: check if succeeded?
    os_memory_commit(base, page_size);

    result = (Arena *)base;
    result->base = base;
    result->offset = align_up(sizeof(Arena));
    result->commit = page_size;
    result->capacity = capacity;
  }

  return result;
}

void me_arena_destroy(Arena *arena) {
  os_memory_release(arena);
  arena = 0;
}

void *me_arena_push(Arena *arena, uint64_t size) {
  void *result = 0;

  uint64_t at = align_up(arena->offset + size);
  ASSERT(at < arena->capacity);

  if (at > arena->commit) {
    uint64_t page_size = os_page_size() * 10;
    void *data = os_memory_commit(arena->base + arena->commit, page_size);
    if (data) {
      result = arena->base + arena->offset;
      arena->offset = at;
      arena->commit += page_size;
    } else {
      // TODO: report error
      fprintf(stderr, "Failed to commit\n");
    }
  } else {
    result = arena->base + arena->offset;
    arena->offset = at;
  }

  return result;
}

void *me_arena_push_zero(Arena *arena, uint64_t size) {
  void *result = me_arena_push(arena, size);
  os_memory_zero(result, size);
  return result;
}

void me_arena_pop(Arena *arena, uint64_t size) {
  uint64_t at = align_up(arena->offset - size);
  ASSERT(at >= 0);

  uint64_t page_size = os_page_size();
  if (at < (arena->commit - page_size)) {
    os_memory_decommit(arena->base + arena->commit, page_size);
    arena->commit -= page_size;
  }
}

