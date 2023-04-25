#ifndef _MEM_H
#define _MEM_H

#include "types.h"

////////////////////////////////////////////////////////////////////////////////

struct arena_t {
  u64 pos;
  u64 cap;
  // TODO: handle growing reserve/commit
};

struct arena_temp_t {
  arena_t *arena;
  u64 pos;
};

////////////////////////////////////////////////////////////////////////////////

arena_t *arena_create(void);
void     arena_reset(arena_t *arena);

void    *arena_push(arena_t *arena, u64 size);
void    *arena_push_zero(arena_t *arena, u64 size);
void     arena_pop_to(arena_t *arena, u64 pos);


arena_temp_t arena_temp_begin(arena_t *arena);
void         arena_temp_end(arena_temp_t temp);

////////////////////////////////////////////////////////////////////////////////

#define PUSH_ARRAY(ARENA,TYPE,COUNT) (TYPE*)arena_push_zero((ARENA), sizeof(TYPE)*(COUNT))

#endif // _MEM_H
