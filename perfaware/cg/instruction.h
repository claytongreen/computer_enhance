#ifndef _INSTRUCTION_H
#define _INSTRUCTION_H

#include "types.h"
#include "string.h"

//
// TODO: generate the opcodes and registers using the preprocessor??

enum op_code_t : u8 {
  OP_CODE_NONE,
  // ---------------------------------------------------------------------------
  OP_CODE_ADD,
  OP_CODE_CMP,
  OP_CODE_DEC,
  OP_CODE_INC,
  OP_CODE_JA,
  OP_CODE_JB,
  OP_CODE_JBE,
  OP_CODE_JCXZ,
  OP_CODE_JE,
  OP_CODE_JG,
  OP_CODE_JL,
  OP_CODE_JLE,
  OP_CODE_JNB,
  OP_CODE_JNL,
  OP_CODE_JNO,
  OP_CODE_JNP,
  OP_CODE_JNS,
  OP_CODE_JNZ,
  OP_CODE_JO,
  OP_CODE_JP,
  OP_CODE_JS,
  OP_CODE_LOOP,
  OP_CODE_LOOPNZ,
  OP_CODE_LOOPZ,
  OP_CODE_MOV,
  OP_CODE_XCHG,
  OP_CODE_POP,
  OP_CODE_PUSH,
  OP_CODE_SUB,
  // ---------------------------------------------------------------------------
  OP_CODE_COUNT,
};

enum register_t : u8 {
  REGISTER_NONE,
  // ---------------------------------------------------------------------------
  REGISTER_AX, REGISTER_AH, REGISTER_AL,
  REGISTER_BX, REGISTER_BH, REGISTER_BL,
  REGISTER_CX, REGISTER_CH, REGISTER_CL,
  REGISTER_DX, REGISTER_DH, REGISTER_DL,

  REGISTER_SP,
  REGISTER_BP,
  REGISTER_SI,
  REGISTER_DI,

  REGISTER_ES,
  REGISTER_CS,
  REGISTER_SS,
  REGISTER_DS,
  // ---------------------------------------------------------------------------
  REGISTER_COUNT,
};

enum operand_kind_t : u8 {
  OPERAND_KIND_NONE,
  // ---------------------------------------------------------------------------
  OPERAND_KIND_REGISTER,
  OPERAND_KIND_ADDRESS,
  OPERAND_KIND_IMMEDIATE,
  OPERAND_KIND_LABEL,
  // ---------------------------------------------------------------------------
  OPERAND_KIND_COUNT,
};


// @TODO: better name
struct address_t {
  register_t registers[2];
  u8 register_count;
  // @TODO: offset8 and offset16, or just a single 16 bit offset?
  s16 offset;
};

struct operand_t {
  operand_kind_t kind;

  union {
    u16 immediate;
    register_t reg;
    address_t address;

    u8 label_index;
  };
};

// TODO: flags: lock, rep, segment, wide, far
enum instruction_flag_t : u8 {
  INSTRUCTION_FLAG_SIGN_EXTEND = 1 << 0,
  INSTRUCTION_FLAG_WIDE        = 1 << 1,
  INSTRUCTION_FLAG_DEST        = 1 << 2,
};

#define INSTRUCTION_MAX_BYTE_COUNT 6
struct instruction_t {
  u8 *ip;
  u8 bytes_count;

  op_code_t opcode;

  // TODO: rename to operand_t operands[2]
  operand_t dest;
  operand_t source;

  u32 flags;
};

static string_t op_code_names[OP_CODE_COUNT] = {
    str_none,
    STRING_LIT("add"),
    STRING_LIT("cmp"),
    STRING_LIT("dec"),
    STRING_LIT("inc"),
    STRING_LIT("ja"),
    STRING_LIT("jb"),
    STRING_LIT("jbe"),
    STRING_LIT("jcxz"),
    STRING_LIT("je"),
    STRING_LIT("jg"),
    STRING_LIT("jl"),
    STRING_LIT("jle"),
    STRING_LIT("jnb"),
    STRING_LIT("jnl"),
    STRING_LIT("jno"),
    STRING_LIT("jnp"),
    STRING_LIT("jns"),
    STRING_LIT("jnz"),
    STRING_LIT("jo"),
    STRING_LIT("jp"),
    STRING_LIT("js"),
    STRING_LIT("loop"),
    STRING_LIT("loopnz"),
    STRING_LIT("loopz"),
    STRING_LIT("mov"),
    STRING_LIT("xchg"),
    STRING_LIT("pop"),
    STRING_LIT("push"),
    STRING_LIT("sub"),
};

static string_t register_names[REGISTER_COUNT] = {
  str_none,
  // ---------------------------------------------------------------------------
  STRING_LIT("ax"), STRING_LIT("ah"), STRING_LIT("al"), STRING_LIT("bx"),
  STRING_LIT("bh"), STRING_LIT("bl"), STRING_LIT("cx"), STRING_LIT("ch"),
  STRING_LIT("cl"), STRING_LIT("dx"), STRING_LIT("dh"), STRING_LIT("dl"),
  // ---------------------------------------------------------------------------
  STRING_LIT("sp"), STRING_LIT("bp"), STRING_LIT("si"), STRING_LIT("di"),
  // ---------------------------------------------------------------------------
  STRING_LIT("es"), STRING_LIT("cs"), STRING_LIT("ss"), STRING_LIT("ds"),
};

static register_t effective_address[2][8] = {
  {
    REGISTER_BX, REGISTER_BX, REGISTER_BP, REGISTER_BP,
    REGISTER_SI, REGISTER_DI, REGISTER_BP, REGISTER_BX
  },
  {
    REGISTER_SI,   REGISTER_DI,   REGISTER_SI,   REGISTER_DI,
    REGISTER_NONE, REGISTER_NONE, REGISTER_NONE, REGISTER_NONE,
  }
};


register_t get_register(u8 reg, u8 w);
string_t get_register_name(u8 reg, u8 w);

#endif // _INSTRUCTION_H
