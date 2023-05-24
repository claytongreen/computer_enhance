#include "string.h"
#include "instruction.h"
#include "sim.h"


static string_t print_operand(arena_t *arena, simulator_t *sim, instruction_t instruction, operand_t operand) {
  string_t result = {0};

  u32 instruction_flags = instruction.flags;

  switch (operand.kind) {
  case OPERAND_KIND_IMMEDIATE: {
    u32 instruction_size = instruction.bytes_count;

    if (instruction_flags & INSTRUCTION_FLAG_JUMP) {
      s8 inc = (s8)operand.immediate;
      inc += instruction_size;
      result = string_pushf(arena, "$%d", (s8)inc);
    } else if (instruction_flags & INSTRUCTION_FLAG_SIGN_EXTEND) {
      result = string_pushf(arena, "%d", (s16)operand.immediate);
    } else {
      result = string_pushf(arena, "%d", operand.immediate);
    }
  } break;
  case OPERAND_KIND_REGISTER: {
    result = register_names[operand.reg];
  } break;
  case OPERAND_KIND_ADDRESS: {
    address_t addr = operand.address;

    string_list_t sb = {0};

    if (instruction_flags & INSTRUCTION_FLAG_SPECIFY_SIZE) {
      string_t size = (instruction_flags & INSTRUCTION_FLAG_WIDE) ? STRING_LIT("word ") : STRING_LIT("byte ");
      string_list_push(arena, &sb, size);
    }

    string_list_push(arena, &sb, STRING_LIT("["));
    if (addr.register_count > 0) {
      string_t reg = register_names[addr.registers[0]];
      string_list_push(arena, &sb, reg);
    }
    if (addr.register_count > 1 && addr.registers[1] != REGISTER_NONE) {
      string_t reg = register_names[addr.registers[1]];
      string_list_pushf(arena, &sb, " + %.*s", STRING_FMT(reg));
    }
    if (addr.offset) {
      s16 offset = addr.offset >= 0 ? addr.offset : -addr.offset;
      if (addr.register_count) {
        string_list_pushf(arena, &sb, " %s %d", addr.offset >= 0 ? "+" : "-", offset);
      } else {
        string_list_pushf(arena, &sb, "%d", offset);
      }
    }
    string_list_push(arena, &sb, STRING_LIT("]"));
    result = string_list_join(arena, &sb, {});
  } break;

  case OPERAND_KIND_NONE:
  case OPERAND_KIND_COUNT:
    break;
  }

  return result;
}

static string_t print_instruction(arena_t *arena, simulator_t *sim, instruction_t instruction) {
  string_t result;

  string_t opcode = op_code_names[instruction.opcode];
  string_t dest = print_operand(arena, sim, instruction, instruction.dest);

  if (instruction.source.kind != OPERAND_KIND_NONE) {
    string_t source = print_operand(arena, sim, instruction, instruction.source);
    result = string_pushf(arena, "%.*s %.*s, %.*s", STRING_FMT(opcode), STRING_FMT(dest), STRING_FMT(source));
  } else {
    result = string_pushf(arena, "%.*s %.*s", STRING_FMT(opcode), STRING_FMT(dest));
  }

  return result;
}

static string_t print_flags(arena_t *arena, u16 flags) {
  string_list_t sb = {};

  if (flags & FLAG_CARRY)            string_list_push(arena, &sb, STRING_LIT("C"));
  if (flags & FLAG_PARITY)           string_list_push(arena, &sb, STRING_LIT("P"));
  if (flags & FLAG_AUXILIARY_CARRY)  string_list_push(arena, &sb, STRING_LIT("A"));
  if (flags & FLAG_ZERO)             string_list_push(arena, &sb, STRING_LIT("Z"));
  if (flags & FLAG_SIGN)             string_list_push(arena, &sb, STRING_LIT("S"));
  if (flags & FLAG_TRAP)             string_list_push(arena, &sb, STRING_LIT("T"));
  if (flags & FLAG_INTERRUPT_ENABLE) string_list_push(arena, &sb, STRING_LIT("I"));
  if (flags & FLAG_DIRECTION)        string_list_push(arena, &sb, STRING_LIT("D"));
  if (flags & FLAG_OVERFLOW)         string_list_push(arena, &sb, STRING_LIT("O"));

  return string_list_join(arena, &sb, {});
}
