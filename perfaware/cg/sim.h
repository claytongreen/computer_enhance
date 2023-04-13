#ifndef _SIM_H
#define _SIM_H

#include "types.h"
#include "string.h"
#include "instruction.h"


struct label_t {
  size_t ip;
  string_t label;
};

// TODO: should the decoder just be a part of the simulator??

#define LABEL_COUNT_MAX 4

struct simulator_t {
  u16 registers[8];
  u16 flags;

  u8 *memory;

  u32 ip;
  string_t instruction_stream;

  // TODO: remove? just for pretty printing
  u32 label_count;
  label_t labels[LABEL_COUNT_MAX];

  string_t error;
};

struct register_map_t {
  b32 memory_pointer;

  s32 index;

  u16 mask;
  u8 shift;
};

enum flag_t {
  FLAG_CARRY           = 1 << 0,
  FLAG_PARITY          = 1 << 2,
  FLAG_AUXILIARY_CARRY = 1 << 4,
  FLAG_ZERO            = 1 << 6,
  FLAG_SIGN            = 1 << 7,
  FLAG_TRAP            = 1 << 8,
  FLAG_INTERRUPT       = 1 << 9,
  FLAG_DIRECTION       = 1 << 10,
  FLAG_OVERFLOW        = 1 << 11,
};

#endif // _SIM_H
