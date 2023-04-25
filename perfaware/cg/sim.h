#ifndef _SIM_H
#define _SIM_H

#include "types.h"
#include "string.h"
#include "instruction.h"


struct label_t {
  size_t ip;
  string_t label;
};

#define LABEL_COUNT_MAX 4
struct simulator_t {
  u16 registers[8];

  u16 flags;

  u8 *memory;
  u8 *code_end;

  u32 ip;

  // TODO: remove? just for pretty printing
  u32 label_count;
  label_t labels[LABEL_COUNT_MAX];

  string_t error;

  arena_t *arena;
};

struct register_map_t {
  b32 memory_pointer;

  s32 index;

  u16 mask;
  u8 shift;
};

enum flag_t : u16 {
  // status flags
  FLAG_CARRY,
  FLAG_PARITY,
  FLAG_AUXILIARY_CARRY,
  FLAG_ZERO,
  FLAG_SIGN,
  FLAG_OVERFLOW,
  // control flags
  FLAG_INTERRUPT_ENABLE,
  FLAG_DIRECTION,
  FLAG_TRAP,
  // ---------------------------------------------------------------------------
  FLAG_COUNT,
};

static string_t flag_names[FLAG_COUNT] = {
  STRING_LIT("CF"),
  STRING_LIT("PF"),
  STRING_LIT("AF"),
  STRING_LIT("ZF"),
  STRING_LIT("SF"),
  STRING_LIT("OF"),
  // ---------------------------------------------------------------------------
  STRING_LIT("Interrupt Enable"),
  STRING_LIT("Direction"),
  STRING_LIT("Trap"),
};


void sim_reset(simulator_t *sim);

#endif // _SIM_H
