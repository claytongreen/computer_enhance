#define _CRT_SECURE_NO_WARNINGS

#include <intrin.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

// MEMORY ---------------------------------------------------------------------
static size_t global_memory_size = 1024 * 1024 * 8;
static u8 *global_memory_base;
static u8 *global_memory;
#define PUSH_ARRAY(TYPE, COUNT)                                                \
  (TYPE *)global_memory;                                                       \
  assert((sizeof(TYPE) * (COUNT)) < global_memory_size &&                      \
         "ALLOC SIZE TOO LARGE");                                              \
  memset(global_memory, 0, sizeof(TYPE) * (COUNT));                            \
  global_memory += sizeof(TYPE) * (COUNT);                                     \
  assert(                                                                      \
      ((size_t)(global_memory - global_memory_base) < global_memory_size) &&   \
      "OUT OF MEMORY")
// MEMORY ---------------------------------------------------------------------

#include "string.cpp"

// OS -------------------------------------------------------------------------
static string_t read_entire_file(char *filename) {
  string_t result = {};

  FILE *file = fopen(filename, "rb");
  if (file != NULL) {
    size_t bytes_read = fread(global_memory, 1, global_memory_size, file);
    if (bytes_read) {
      result.data = global_memory;
      result.length = bytes_read;
      global_memory += bytes_read;
    }
    fclose(file);
  }

  return result;
}
// OS -------------------------------------------------------------------------

// PROGRAM --------------------------------------------------------------------

struct label_t {
  size_t ip;
  string_t label;
};

#define LABEL_COUNT_MAX 4
struct decoder_t {
  u8 *start;

  string_t instruction_stream;

  u32 label_count;
  label_t labels[LABEL_COUNT_MAX];

  string_t error;
};

static string_t str_empty = STRING_LIT("");
static string_t str_none = STRING_LIT("none");
// TODO: remove
static string_t str_unknown = STRING_LIT("???");

enum op_code_t {
  OP_CODE_NONE,
  // ------------------
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
  // ---------------------
  OP_CODE_COUNT,
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

enum register_t : u8 {
  REGISTER_NONE,
  // --------------
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
  // --------------
  REGISTER_COUNT,
};

static string_t register_names[REGISTER_COUNT] = {
    str_none,

    STRING_LIT("ax"), STRING_LIT("ah"), STRING_LIT("al"), STRING_LIT("bx"),
    STRING_LIT("bh"), STRING_LIT("bl"), STRING_LIT("cx"), STRING_LIT("ch"),
    STRING_LIT("cl"), STRING_LIT("dx"), STRING_LIT("dh"), STRING_LIT("dl"),

    STRING_LIT("sp"), STRING_LIT("bp"), STRING_LIT("si"), STRING_LIT("di"),

    STRING_LIT("es"), STRING_LIT("cs"), STRING_LIT("ss"), STRING_LIT("ds"),
};

// @TODO: better name
struct address_t {
  register_t registers[2];
  u8 register_count;
  // @TODO: offset8 and offset16, or just a single 16 bit offset?
  s16 offset;
};

enum instruction_flag_t {
  INSTRUCTION_FLAG_DEST = 1 << 0,
  INSTRUCTION_FLAG_WIDE = 1 << 1,
};

enum operand_kind_t {
  OPERAND_KIND_NONE,
  OPERAND_KIND_REGISTER,
  OPERAND_KIND_ADDRESS,
  OPERAND_KIND_IMMEDIATE,
  OPERAND_KIND_IMMEDIATE8,
  OPERAND_KIND_IMMEDIATE16,
  OPERAND_KIND_LABEL,
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

#define INSTRUCTION_MAX_BYTE_COUNT 6
struct instruction_t {
  op_code_t opcode;

  u8 *ip;
  u8 bytes_count;

  operand_t dest;
  operand_t source;
};

struct simulator_t {
  u16 registers[8];
  u16 flags;
  u8 *memory;

  string_t error;
};

// REG | W=0 | W=1
// 000 |  AL |  AX
// 001 |  CL |  CX
// 010 |  DL |  DX
// 011 |  BL |  BX
// 100 |  AH |  SP
// 101 |  CH |  BP
// 110 |  DH |  SI
// 111 |  BH |  DI
static register_t get_register(u8 reg, u8 w) {
  static register_t registers[2][8] = {
      {REGISTER_AL, REGISTER_CL, REGISTER_DL, REGISTER_BL, REGISTER_AH, REGISTER_CH, REGISTER_DH, REGISTER_BH},
      {REGISTER_AX, REGISTER_CX, REGISTER_DX, REGISTER_BX, REGISTER_SP, REGISTER_BP, REGISTER_SI, REGISTER_DI},
  };
  register_t result = registers[w][reg];
  return result;
}

static string_t get_register_name(u8 reg, u8 w) {
  register_t r = get_register(reg, w);
  return register_names[r];
}

static u8 next_byte(string_t *instruction_stream) {
  assert(instruction_stream->length > 0 &&
         "Unexpected end of instruction stream");

  u8 byte = *(instruction_stream->data);
  instruction_stream->data += 1;
  instruction_stream->length -= 1;

  return byte;
}

static s16 next_data_s16(string_t *instruction_stream) {
  u8 data = (s16)next_byte(instruction_stream);
  return data;
}

static u16 next_data_u16(string_t *instruction_stream, u8 w) {
  u8 lo = next_byte(instruction_stream);
  u8 hi = (w == 1) ? next_byte(instruction_stream) : ((lo & 0x80) ? 0xff : 0);

  u16 data = (((s16)hi) << 8) | (u16)lo;

  return data;
}

static register_t effective_address_one[8] = {
    REGISTER_BX, REGISTER_BX, REGISTER_BP, REGISTER_BP,
    REGISTER_SI, REGISTER_DI, REGISTER_BP, REGISTER_BX};
static register_t effective_address_two[8] = {
    REGISTER_SI,   REGISTER_DI,   REGISTER_SI,   REGISTER_DI,
    REGISTER_NONE, REGISTER_NONE, REGISTER_NONE, REGISTER_NONE};
static operand_t next_address(string_t *instruction_stream, u8 w, u8 mod,
                              u8 rm) {
  operand_t result = {};

  if (mod == 3) {
    result.kind = OPERAND_KIND_REGISTER;
    result.reg = get_register(rm, w);
  } else if (mod == 0 && rm == 6) {
    // @TODO: next_data_s16?
    result.kind = OPERAND_KIND_ADDRESS;
    result.address.offset = next_data_u16(instruction_stream, 1);
  } else {
    result.kind = OPERAND_KIND_ADDRESS;
    result.address.registers[0] = effective_address_one[rm];
    result.address.registers[1] = effective_address_two[rm];
    result.address.register_count = 2;
    if (mod) {
      // @TODO: next_data_s16?
      result.address.offset =
          next_data_u16(instruction_stream, mod == 2 ? 1 : 0);
    }
  }

  return result;
}

static instruction_t instruction_decode(decoder_t *decoder, size_t offset) {
  instruction_t result = {};

  string_t *instruction_stream = &decoder->instruction_stream;

  u8 b1 = next_byte(instruction_stream);

  switch (b1) {
  case 0x00: // ADD b,f,r/m
  case 0x01: // ADD w,f,r/m
  case 0x02: // ADD b,t,r/m
  case 0x03: // ADD w,t,r/m
  case 0x28: // SUB b,f,r/m
  case 0x29: // SUB w,f,r/m
  case 0x2A: // SUB b,t,r/m
  case 0x2B: // SUB w,t,r/m
  case 0x38: // CMP b,f,r/m
  case 0x39: // CMP w,f,r/m
  case 0x3A: // CMP b,t,r/m
  case 0x3B: // CMP w,t,r/m
  case 0x88: // MOV b,f,r/m
  case 0x89: // MOV w,f,r/m
  case 0x8A: // MOV b,t,r/m
  case 0x8B: // MOV w,t,r/m
  {
    u8 d = (b1 >> 1) & 1;
    u8 w = (b1 >> 0) & 1;

    u8 b2 = next_byte(instruction_stream);
    u8 mod = (b2 >> 6);
    u8 reg = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    operand_t op_address = next_address(instruction_stream, w, mod, rm);

    switch (b1) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
      result.opcode = OP_CODE_ADD;
      break;

    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
      result.opcode = OP_CODE_SUB;
      break;

    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
      result.opcode = OP_CODE_CMP;
      break;

    case 0x88:
    case 0x89:
    case 0x8A:
    case 0x8B:
      result.opcode = OP_CODE_MOV;
      break;

    default:
      decoder->error = string_pushf("\n!!! Unexpected opcode 0x%x\n", b1);
      break;
    }

    operand_t op_register = {OPERAND_KIND_REGISTER};
    op_register.reg = get_register(reg, w);

    result.dest = d ? op_register : op_address;
    result.source = d ? op_address : op_register;
  } break;

  case 0x04: // ADD b,ia
  case 0x05: // ADD w,ia
  case 0x2C: // SUB b,ia
  case 0x2D: // SUB w,ia
  case 0x3C: // CMP b,ia
  case 0x3D: // CMP w,ia
  {
    u8 w = (b1 >> 0) & 1;

    u16 data = next_data_u16(instruction_stream, w);

    switch (b1) {
    case 0x04:
    case 0x05:
      result.opcode = OP_CODE_ADD;
      break;

    case 0x2C:
    case 0x2D:
      result.opcode = OP_CODE_SUB;
      break;

    case 0x3C:
    case 0x3D:
      result.opcode = OP_CODE_CMP;
      break;

    default:
      decoder->error = string_pushf("\n!!! Unexpected opcode 0x%02x\n", b1);
      break;
    }
    operand_t dest = {OPERAND_KIND_REGISTER};
    switch (b1) {
    case 0x04:
    case 0x2C:
    case 0x3C:
      dest.reg = REGISTER_AL;
      break;

    case 0x05:
    case 0x2D:
    case 0x3D:
      dest.reg = REGISTER_AX;
      break;

    default:
      decoder->error = string_pushf("\n!!! Unexpected dest 0x%02x\n", b1);
      break;
    }

    operand_t source = {OPERAND_KIND_IMMEDIATE};
    source.immediate = data;

    result.dest = dest;
    result.source = source;
  } break;

  case 0x0E: // PUSH CS
  {
    result.opcode = OP_CODE_PUSH;
    result.dest.kind = OPERAND_KIND_REGISTER;
    result.dest.reg = REGISTER_CS;
  } break;

  case 0x50: // PUSH AX
  case 0x51: // PUSH CX
  case 0x52: // PUSH DX
  case 0x53: // PUSH BX
  case 0x54: // PUSH SP
  case 0x55: // PUSH BP
  case 0x56: // PUSH SI
  case 0x57: // PUSH DI
  {
    register_t reg = (register_t)((s32)REGISTER_AX + (b1 - 0x50));

    result.opcode = OP_CODE_PUSH;
    result.dest.kind = OPERAND_KIND_REGISTER;
    result.dest.reg = reg;
  } break;

  case 0x1E: // PUSH DS
  {
    result.opcode = OP_CODE_PUSH;
    result.dest.kind = OPERAND_KIND_REGISTER;
    result.dest.reg = REGISTER_DS;
  } break;

  case 0x1F: // POP DS
  {
    result.opcode = OP_CODE_POP;
    result.dest.kind = OPERAND_KIND_REGISTER;
    result.dest.reg = REGISTER_DS;
  } break;

  case 0x58: // POP AX
  case 0x59: // POP CX
  case 0x5A: // POP DX
  case 0x5B: // POP BX
  case 0x5C: // POP SP
  case 0x5D: // POP BP
  case 0x5E: // POP SI
  case 0x5F: // POP DI
  {
    register_t reg = (register_t)((int)REGISTER_AX + (b1 - 0x58));

    result.opcode = OP_CODE_POP;
    result.dest.kind = OPERAND_KIND_REGISTER;
    result.dest.reg = reg;
  } break;

  case 0x8F: // POP r/m
  {
    u8 b2 = next_byte(instruction_stream);
    u8 mod = (b2 >> 6);
    // u8 reg = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    operand_t address = next_address(instruction_stream, 0, mod, rm);

    result.opcode = OP_CODE_POP;
    result.dest = address; // TODO: WORD?
  } break;

  case 0x87: // XCHG w,r/m
  {
    u8 w = (b1 >> 0) & 1;

    u8 b2 = next_byte(instruction_stream);
    u8 mod = (b2 >> 6);
    u8 reg = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    operand_t address = next_address(instruction_stream, w, mod, rm);

    result.opcode = OP_CODE_XCHG;
    result.dest.kind = OPERAND_KIND_REGISTER;
    result.dest.reg = get_register(reg, w);
    result.source = address;
  } break;

  case 0x70: // JO
  case 0x71: // JNO
  case 0x72: // JB/JNAE
  case 0x73: // JNB/JAE
  case 0x74: // JE/JZ
  case 0x76: // JBE/JNA
  case 0x75: // JNE/JNZ
  case 0x77: // JNBE/JA
  case 0x78: // JS
  case 0x79: // JNS
  case 0x7A: // JP/JPE
  case 0x7B: // JNP/JPO
  case 0x7C: // JL/JNGE
  case 0x7D: // JNL/JGE
  case 0x7E: // JLE/JNG
  case 0x7F: // JNLE/JG
  case 0xE0: // LOOPNZ/LOOPNE
  case 0xE1: // LOOPZ/LOOPE
  case 0xE2: // LOOP
  case 0xE3: // JCXZ
  {
    s8 inc = (s8)next_byte(instruction_stream);

    s8 label_index = -1;

    size_t label_ip = (instruction_stream->data - decoder->start) + inc;
    if (decoder->label_count < LABEL_COUNT_MAX) { // have space for more labels
      // look for existing label
      for (size_t i = 0; i < decoder->label_count; i += 1) {
        if (decoder->labels[i].ip == label_ip) {
          label_index = i;
          break;
        }
      }

      if (label_index == -1) {
        string_t label = string_pushf("label%d", label_ip);
        label_index = decoder->label_count++;
        decoder->labels[label_index] = {label_ip, label};
      }
    } else {
      decoder->error = STRING_LIT("Too many labels");
    }

    // TODO: Is there a nice way to map this?
    switch (b1) {
    case 0x70:
      result.opcode = OP_CODE_JO;
      break;
    case 0x71:
      result.opcode = OP_CODE_JNO;
      break;
    case 0x72:
      result.opcode = OP_CODE_JB;
      break;
    case 0x73:
      result.opcode = OP_CODE_JNB;
      break;
    case 0x74:
      result.opcode = OP_CODE_JE;
      break;
    case 0x75:
      result.opcode = OP_CODE_JNZ;
      break;
    case 0x76:
      result.opcode = OP_CODE_JBE;
      break;
    case 0x77:
      result.opcode = OP_CODE_JA;
      break;
    case 0x78:
      result.opcode = OP_CODE_JS;
      break;
    case 0x79:
      result.opcode = OP_CODE_JNS;
      break;
    case 0x7A:
      result.opcode = OP_CODE_JP;
      break;
    case 0x7B:
      result.opcode = OP_CODE_JNP;
      break;
    case 0x7C:
      result.opcode = OP_CODE_JL;
      break;
    case 0x7D:
      result.opcode = OP_CODE_JNL;
      break;
    case 0x7E:
      result.opcode = OP_CODE_JLE;
      break;
    case 0x7F:
      result.opcode = OP_CODE_JG;
      break;
    case 0xE0:
      result.opcode = OP_CODE_LOOPNZ;
      break;
    case 0xE1:
      result.opcode = OP_CODE_LOOPZ;
      break;
    case 0xE2:
      result.opcode = OP_CODE_LOOP;
      break;
    case 0xE3:
      result.opcode = OP_CODE_JCXZ;
      break;

    default:
      decoder->error = string_pushf("\n!!! Unexpected opcode 0x%02x\n", b1);
      break;
    }

    result.dest.kind = OPERAND_KIND_LABEL;
    result.dest.label_index = label_index;
  } break;

  case 0x80: // Immed b,r/m
  case 0x81: // Immed w,r/m
  case 0x82: // Immed b,r/m
  case 0x83: // Immed is,r/m
  {
    u8 w = (b1 >> 0) & 1;

    u8 b2 = next_byte(instruction_stream);
    u8 mod = (b2 >> 6);
    u8 op = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    operand_t dest = next_address(instruction_stream, w, mod, rm);

    u16 data = b1 == 0x83 ? next_data_s16(instruction_stream)
                          : next_data_u16(instruction_stream, w);

    switch (op) {
    case 0:
      result.opcode = OP_CODE_ADD;
      break;
    case 1:
      decoder->error = STRING_LIT("Invalid op: 1");
      break;
    case 2:
      decoder->error = STRING_LIT("TODO: adc");
      break;
    case 3:
      decoder->error = STRING_LIT("TODO: sbb");
      break;
    case 4:
      decoder->error = STRING_LIT("Invalid op: 4");
      break;
    case 5:
      result.opcode = OP_CODE_SUB;
      break;
    case 6:
      decoder->error = STRING_LIT("Invalid op: 6");
      break;
    case 7:
      result.opcode = OP_CODE_CMP;
      break;
    }

    result.source.kind =
        (mod == 3) ? OPERAND_KIND_IMMEDIATE
                   : (w ? OPERAND_KIND_IMMEDIATE16 : OPERAND_KIND_IMMEDIATE8);
    result.source.immediate = data;

    result.dest = dest;

  } break;

  case 0x8C: // MOV sr,f,r/m
  {
    u8 b2 = next_byte(instruction_stream);

    // sr: 00=ES, 01=CS, 10=SS, 11=DS

    u8 mod = (b2 >> 6) & 3;
    u8 sr = (b2 >> 3) & 3;
    u8 rm = (b2 >> 0) & 7;

    operand_t dest = next_address(instruction_stream, 1, mod, rm);

    register_t reg = (register_t)(REGISTER_ES + sr);

    result.opcode = OP_CODE_MOV;
    result.dest = dest;
    result.source.kind = OPERAND_KIND_REGISTER;
    result.source.reg = reg;
  } break;
  case 0x8E: // MOV sr,t,r/m
  {
    u8 b2 = next_byte(instruction_stream);

    // sr: 00=ES, 01=CS, 10=SS, 11=DS

    u8 mod = (b2 >> 6) & 3;
    u8 sr = (b2 >> 3) & 3;
    u8 rm = (b2 >> 0) & 7;

    operand_t source = next_address(instruction_stream, 1, mod, rm);

    register_t reg = (register_t)(REGISTER_ES + sr);

    result.opcode = OP_CODE_MOV;
    result.dest.kind = OPERAND_KIND_REGISTER;
    result.dest.reg = reg;
    result.source = source;
  } break;

  case 0xA0: // MOV m -> AL
  case 0xA1: // MOV m -> AX
  case 0xA2: // MOV AX -> m
  case 0xA3: // MOV AX -> m
  {
    u8 d = (b1 >> 1) & 1;
    u8 w = (b1 >> 0) & 1;

    operand_t op_addr = next_address(instruction_stream, 0, 0, 6);

    operand_t op_reg = {OPERAND_KIND_REGISTER};
    op_reg.reg = w ? REGISTER_AX : REGISTER_AL;

    result.opcode = OP_CODE_MOV;
    result.dest = d ? op_addr : op_reg;
    result.source = d ? op_reg : op_addr;
  } break;

  case 0xB0: // MOV i -> AL
  case 0xB1: // MOV i -> CL
  case 0xB2: // MOV i -> DL
  case 0xB3: // MOV i -> BL
  case 0xB4: // MOV i -> AH
  case 0xB5: // MOV i -> CH
  case 0xB6: // MOV i -> DH
  case 0xB7: // MOV i -> BH
  case 0xB8: // MOV i -> AX
  case 0xB9: // MOV i -> CX
  case 0xBA: // MOV i -> DX
  case 0xBB: // MOV i -> BX
  case 0xBC: // MOV i -> SP
  case 0xBD: // MOV i -> BP
  case 0xBE: // MOV i -> SI
  case 0xBF: // MOV i -> DI
  {
    u8 w = b1 >= 0xB8; // TODO: does this "scale" (i.e. work for all values?
                       // probably not)

    u16 data = next_data_u16(instruction_stream, w);

    operand_t dest = {OPERAND_KIND_REGISTER};
    // dest.reg = (register_t)(b1 - 0xB0 + 1);
    dest.reg = get_register((b1 - 0xB0) % 8, w);

    operand_t source = {OPERAND_KIND_IMMEDIATE};
    source.immediate = data;

    result.opcode = OP_CODE_MOV;
    result.dest = dest;
    result.source = source;
  } break;

  case 0xC6: // MOV b,i,r/m
  case 0xC7: // MOV w,i,r/m
  {
    u8 w = (b1 >> 0) & 1;

    u8 b2 = next_byte(instruction_stream);
    u8 mod = (b2 >> 6);
    // TODO: op?
    u8 rm = (b2 >> 0) & 7;

    operand_t op_addr = next_address(instruction_stream, w, mod, rm);
    u16 data = next_data_u16(instruction_stream, w);

    result.opcode = OP_CODE_MOV;

    result.dest = op_addr;

    result.source.kind = w ? OPERAND_KIND_IMMEDIATE16 : OPERAND_KIND_IMMEDIATE8;
    result.source.immediate = data;

  } break;

  // INC, DEC, CALL id, CALL l id, JMP id, PUSH, -
  case 0xFF: // Grp 2 w,r/m
  {
    u8 b2 = next_byte(instruction_stream);
    u8 mod = (b2 >> 6);
    u8 op = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    // PUSH MEM16
    if (op == 6) {
      result.opcode = OP_CODE_PUSH;

      operand_t address = next_address(instruction_stream, 1, mod, rm);
      result.dest = address;
    }
    // INC  MEM16
    else if (op == 0) {
      result.opcode = OP_CODE_INC;
      operand_t address = next_address(instruction_stream, 1, mod, rm);
      result.dest = address;
      // case 1: break; // DEC  MEM16
      // case 2: break; // CALL REG16/MEM16 (intra)
      // case 3: break; // CALL MEM16 (intersegment)
      // case 4: break; // JMP  REG16/MEM16 (intersegment)
      // case 7: assert(false); break; // (not used)
    } else {
      decoder->error =
          string_pushf("Unexpected op: 0b%.*s", 3, &bit_string_u8(op)[6]);
    }
  } break;

  default:
    decoder->error =
        string_pushf("Unexpected instruction 0b%s", bit_string_u8(b1));
    break;
  }

  return result;
}

static string_t operand_print(decoder_t *decoder, operand_t operand) {
  string_t result = {0};

  switch (operand.kind) {
  case OPERAND_KIND_IMMEDIATE: {
    result = string_pushf("%d", operand.immediate);
  } break;
  case OPERAND_KIND_IMMEDIATE8: {
    result = string_pushf("byte %d", operand.immediate);
  } break;
  case OPERAND_KIND_IMMEDIATE16: {
    result = string_pushf("word %d", operand.immediate);
  } break;
  case OPERAND_KIND_REGISTER: {
    result = register_names[operand.reg];
  } break;
  case OPERAND_KIND_ADDRESS: {
    address_t addr = operand.address;

    string_list_t sb = {0};
    string_list_push(&sb, STRING_LIT("["));
    if (addr.register_count > 0) {
      string_t reg = register_names[addr.registers[0]];
      string_list_push(&sb, reg);
    }
    if (addr.register_count > 1 && addr.registers[1] != REGISTER_NONE) {
      string_t reg = register_names[addr.registers[1]];
      string_list_pushf(&sb, " + %.*s", STRING_FMT(reg));
    }
    if (addr.offset) {
      s16 offset = addr.offset >= 0 ? addr.offset : -addr.offset;
      if (addr.register_count) {
        string_list_pushf(&sb, " %s %d", addr.offset >= 0 ? "+" : "-", offset);
      } else {
        string_list_pushf(&sb, "%d", offset);
      }
    }
    string_list_push(&sb, STRING_LIT("]"));
    result = string_list_join(&sb);
  } break;
  case OPERAND_KIND_LABEL: {
    label_t label = decoder->labels[operand.label_index];
    result = string_pushf("%.*s ; %d", STRING_FMT(label.label), label.ip);
  } break;

  case OPERAND_KIND_NONE:
    break;
  }

  return result;
}

static string_t instruction_print(decoder_t *decoder,
                                  instruction_t instruction) {
  string_t result;

  string_t opcode = op_code_names[instruction.opcode];
  string_t dest = operand_print(decoder, instruction.dest);

  if (instruction.source.kind != OPERAND_KIND_NONE) {
    string_t source = operand_print(decoder, instruction.source);
    result = string_pushf("%.*s %.*s, %.*s", STRING_FMT(opcode),
                          STRING_FMT(dest), STRING_FMT(source));
  } else {
    result = string_pushf("%.*s %.*s", STRING_FMT(opcode), STRING_FMT(dest));
  }

  return result;
}

struct register_map_t {
  b32 memory_pointer;

  s32 index;

  u16 mask;
  u8 shift;
};
static register_map_t register_map[REGISTER_COUNT];

static void init_register_map() {
  register_map[REGISTER_AX] = {0, 0, 0xFFFF, 0};
  register_map[REGISTER_AH] = {0, 0, 0xFF00, 8};
  register_map[REGISTER_AL] = {0, 0, 0x00FF, 0};

  register_map[REGISTER_BX] = {0, 1, 0xFFFF, 0};
  register_map[REGISTER_BH] = {0, 1, 0xFF00, 8};
  register_map[REGISTER_BL] = {0, 1, 0x00FF, 0};

  register_map[REGISTER_CX] = {0, 2, 0xFFFF, 0};
  register_map[REGISTER_CH] = {0, 2, 0xFF00, 8};
  register_map[REGISTER_CL] = {0, 2, 0x00FF, 0};

  register_map[REGISTER_DX] = {0, 3, 0xFFFF, 0};
  register_map[REGISTER_DH] = {0, 3, 0xFF00, 8};
  register_map[REGISTER_DL] = {0, 3, 0x00FF, 0};

  register_map[REGISTER_SP] = {0, 4, 0xFFFF, 0};
  register_map[REGISTER_BP] = {0, 5, 0xFFFF, 0};
  register_map[REGISTER_SI] = {0, 6, 0xFFFF, 0};
  register_map[REGISTER_DI] = {0, 7, 0xFFFF, 0};

  // TODO: program sets base pointers for segment registers
  register_map[REGISTER_ES] = {1, 64 * 1024 * 0};
  register_map[REGISTER_CS] = {1, 64 * 1024 * 1};
  register_map[REGISTER_SS] = {1, 64 * 1024 * 3};
  register_map[REGISTER_DS] = {1, 64 * 1024 * 4};

  register_map[REGISTER_NONE] = {0, -1};
}

// TODO: there's got to be a better way
static register_t register_remap[REGISTER_COUNT] = {
    REGISTER_NONE,

    REGISTER_AX,   REGISTER_AX, REGISTER_AX, REGISTER_BX,
    REGISTER_BX,   REGISTER_BX, REGISTER_CX, REGISTER_CX,
    REGISTER_CX,   REGISTER_DX, REGISTER_DX, REGISTER_DX,

    REGISTER_SP,   REGISTER_BP, REGISTER_SI, REGISTER_DI,

    REGISTER_ES,   REGISTER_CS, REGISTER_SS, REGISTER_DS,
};

static void register_set(simulator_t *sim, register_t reg, u16 value) {
  register_map_t map = register_map[reg];
  assert(map.index != -1 && "TODO: handle register_set");

  if (map.memory_pointer) {
    u8 *p = sim->memory + map.index;
    u16 *p16 = (u16 *)p;
    *p16 = value;
  } else {
    sim->registers[map.index] = (sim->registers[map.index] & ~map.mask) |
                                ((value << map.shift) & map.mask);
  }
}

static u16 register_get(simulator_t *sim, register_t reg) {
  u16 result = 0;

  register_map_t map = register_map[register_remap[reg]];

  if (map.memory_pointer) {
    result = *(u16 *)(sim->memory + map.index);
  } else {
    result = (sim->registers[map.index] & map.mask) >> map.shift;
  }

  return result;
}

//          1
//          2631
//          84268421
// ____ODIT SZ_A_P_C
// ____FFFF FF_F_F_F

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

static string_t print_flags(u16 flags) {
  string_list_t sb = {};

  if (flags & FLAG_CARRY)            string_list_push(&sb, STRING_LIT("C"));
  if (flags & FLAG_PARITY)           string_list_push(&sb, STRING_LIT("P"));
  if (flags & FLAG_AUXILIARY_CARRY)  string_list_push(&sb, STRING_LIT("A"));
  if (flags & FLAG_ZERO)             string_list_push(&sb, STRING_LIT("Z"));
  if (flags & FLAG_SIGN)             string_list_push(&sb, STRING_LIT("S"));
  if (flags & FLAG_TRAP)             string_list_push(&sb, STRING_LIT("T"));
  if (flags & FLAG_INTERRUPT)        string_list_push(&sb, STRING_LIT("I"));
  if (flags & FLAG_DIRECTION)        string_list_push(&sb, STRING_LIT("D"));
  if (flags & FLAG_OVERFLOW)         string_list_push(&sb, STRING_LIT("O"));

  return string_list_join(&sb);
}

static void flag_set(u16 *flags, flag_t flag, b32 value) {
  if (value) *flags |=  flag;
  else       *flags &= ~flag;
}

// NOTE(cg): comments showing current regsiter state always reference full 16bit
// register, never high/low portion
static string_t instruction_simulate(simulator_t *sim, decoder_t *decoder,
                                     instruction_t instruction) {
  u16 flags_before = sim->flags;
  u16 registers_before[8];
  for (u8 i = 0; i < 8; i += 1) {
    registers_before[i] = sim->registers[i];
  }


  string_list_t sb = {};
  string_list_push(&sb, instruction_print(decoder, instruction));

  register_t reg = REGISTER_NONE;
  u16 reg_before = 0;

  switch (instruction.opcode) {
  case OP_CODE_MOV: {
    reg = instruction.dest.reg;

    // MOV ax, 1
    if (instruction.source.kind == OPERAND_KIND_IMMEDIATE) {
      u16 value = instruction.source.immediate;
      reg_before = register_get(sim, reg);
      register_set(sim, reg, value);
    }
    // MOV ax, bx
    else if (instruction.source.kind == OPERAND_KIND_REGISTER) {
      u16 value = register_get(sim, instruction.source.reg);
      reg_before = register_get(sim, reg);
      register_set(sim, reg, value);
    } else {
      sim->error = STRING_LIT("ERROR: simulate: Unhandled MOV");
      break;
    }

  } break;

  case OP_CODE_ADD:
  case OP_CODE_SUB: {
    reg = instruction.dest.reg;

    u16 src_value = 0;

    // ADD, SUB ax, 1
    if (instruction.source.kind == OPERAND_KIND_IMMEDIATE) {
      src_value = instruction.source.immediate;
    }
    // ADD, SUB ax, bx
    else if (instruction.source.kind == OPERAND_KIND_REGISTER) {
      src_value = register_get(sim, instruction.source.reg);
    } else {
      sim->error = STRING_LIT("ERROR: simulate: unhandled SUB");
      break;
    }

    u16 value = register_get(sim, reg);
    if (instruction.opcode == OP_CODE_ADD) {
      value += src_value;
    } else {
      value -= src_value;
    }

    reg_before = register_get(sim, reg);
    register_set(sim, reg, value);

    b32 parity = 1;
    for (u8 i = 0; i < 16; i += 1) {
      if ((value >> i) & 1) {
        parity = ~parity;
      }
    }

    flag_set(&sim->flags, FLAG_AUXILIARY_CARRY, 0); // TODO
    flag_set(&sim->flags, FLAG_CARRY, 0); // TODO
    flag_set(&sim->flags, FLAG_OVERFLOW, 0); // TODO
    flag_set(&sim->flags, FLAG_PARITY, parity); // TODO
    flag_set(&sim->flags, FLAG_SIGN, 0x8000 & value);
    flag_set(&sim->flags, FLAG_ZERO, value == 0);
  } break;

  case OP_CODE_CMP: {
    u16 src_value = 0;

    if (instruction.source.kind == OPERAND_KIND_REGISTER) {
      src_value = register_get(sim, instruction.source.reg);
    } else {
      sim->error = STRING_LIT("ERROR: simulate: unhandled CMP");
      break;
    }

    register_t dest = instruction.dest.reg;
    u16 dest_val = register_get(sim, dest);
    u16 value = dest_val - src_value;

    b32 parity = 1;
    for (u8 i = 0; i < 16; i += 1) {
      if ((value >> i) & 1) {
        parity = ~parity;
      }
    }

    flag_set(&sim->flags, FLAG_AUXILIARY_CARRY, 0); // TODO
    flag_set(&sim->flags, FLAG_CARRY, 0); // TODO
    flag_set(&sim->flags, FLAG_OVERFLOW, 0); // TODO
    flag_set(&sim->flags, FLAG_PARITY, parity); // TODO
    flag_set(&sim->flags, FLAG_SIGN, 0x8000 & value);
    flag_set(&sim->flags, FLAG_ZERO, value == 0);
  } break;

  case OP_CODE_NONE:
  case OP_CODE_DEC:
  case OP_CODE_INC:
  case OP_CODE_JA:
  case OP_CODE_JB:
  case OP_CODE_JBE:
  case OP_CODE_JCXZ:
  case OP_CODE_JE:
  case OP_CODE_JG:
  case OP_CODE_JL:
  case OP_CODE_JLE:
  case OP_CODE_JNB:
  case OP_CODE_JNL:
  case OP_CODE_JNO:
  case OP_CODE_JNP:
  case OP_CODE_JNS:
  case OP_CODE_JNZ:
  case OP_CODE_JO:
  case OP_CODE_JP:
  case OP_CODE_JS:
  case OP_CODE_LOOP:
  case OP_CODE_LOOPNZ:
  case OP_CODE_LOOPZ:
  case OP_CODE_XCHG:
  case OP_CODE_POP:
  case OP_CODE_PUSH:
  case OP_CODE_COUNT: {
    sim->error = string_pushf("ERROR: simulate: unhandled opcode: %.*s",
                              STRING_FMT(op_code_names[instruction.opcode]));
  } break;
  }

  if (sim->error.length == 0) {
    string_list_push(&sb, STRING_LIT(" ; "));

    if (reg != REGISTER_NONE) {
      u16 reg_after = register_get(sim, reg);
      string_list_pushf(&sb, "%.*s:0x%x->0x%x ", STRING_FMT(register_names[reg]), reg_before, reg_after);
    }

    if (flags_before != sim->flags) {
      string_t before = print_flags(flags_before);
      string_t after = print_flags(sim->flags);
      string_list_pushf(&sb, "flags:%.*s->%.*s ", STRING_FMT(before), STRING_FMT(after));
    }
  }

  return string_list_join(&sb);
}

static void print_usage(char *exe) {
  fprintf(stderr, "Usage: %s <filename>\n", exe);
}

int main(int argc, char **argv) {
  if (argc == 1) {
    print_usage(argv[0]);
    return 1;
  }

  b32 decode = 0;
  b32 simulate = 0;
  b32 verbose = 0;

  char *filepath = 0;

  for (s32 i = 1; i < argc; i += 1) {
    char *arg = argv[i];
    if (arg[0] == '-') {
      arg = &arg[1];
      if (strcmp(arg, "decode") == 0) {
        decode = 1;
      } else if (strcmp(arg, "exec") == 0) {
        simulate = 1;
      } else if (strcmp(arg, "v") == 0) {
        verbose = 1;
      }
    } else {
      filepath = arg;
    }
  }

  if (!filepath) {
    print_usage(argv[0]);
    return 1;
  }
  if (!decode && !simulate) {
    print_usage(argv[0]);
    return 1;
  }

  global_memory_base = (u8 *)malloc(global_memory_size);
  assert(global_memory_base && "Failed to allocate data");
  memset(global_memory_base, 0, global_memory_size);
  global_memory = global_memory_base;

  decoder_t decoder = {0};

  decoder.instruction_stream = read_entire_file(filepath);
  if (decoder.instruction_stream.length == 0) {
    fprintf(stderr, "Failed to open %s\n", filepath);
    return 1;
  }
  assert(decoder.instruction_stream.length != 0 && "Failed to read file");
  decoder.start = decoder.instruction_stream.data;

  simulator_t sim = {0};
  sim.memory = PUSH_ARRAY(u8, 1024 * 1024);

  init_register_map();

  string_list_t parts = string_split(string_cstring(filepath), '\\');
  string_t filename = parts.last->string;

  struct cmd_t {
    cmd_t *first;

    cmd_t *next;
    cmd_t *prev;

    u8 *start;
    u8 *end;
    string_t text;
  };

  cmd_t *last_cmd = NULL;

  for (;;) {
    if (decoder.instruction_stream.length == 0)
      break;

    u8 *instruction_stream_start = decoder.instruction_stream.data;

    instruction_t instruction = instruction_decode(&decoder, 0);

    // TODO: handle "simulating" instructions

    string_t text;
    if (decoder.error.length) {
      text = string_pushf("??? ; %.*s", STRING_FMT(decoder.error));
    } else if (simulate) {
      text = instruction_simulate(&sim, &decoder, instruction);
      if (sim.error.length) {
        text = string_pushf(" ; %.*s", STRING_FMT(sim.error));
      }
    } else {
      text = instruction_print(&decoder, instruction);
    }

    // @TODO: this is only here because of labeled jmp's. But if we don't do
    // those, we can just print directly.
    cmd_t *cmd = PUSH_ARRAY(cmd_t, 1);
    cmd->start = instruction_stream_start;
    cmd->end = decoder.instruction_stream.data;
    cmd->text = text;

    if (last_cmd) {
      cmd->prev = last_cmd;
      cmd->first = last_cmd->first;
      last_cmd->next = cmd;
    } else {
      cmd->first = cmd;
    }
    last_cmd = cmd;

    if (decoder.error.length)
      break;
    if (sim.error.length)
      break;
  }

  // Print the stuff

  if (simulate) {
    printf("--- test\\%.*s execution ---\n", STRING_FMT(filename));
  } else {
    printf("; %.*s\n", STRING_FMT(filename));
    if (verbose) {
      printf("; %zd bytes\n", (size_t)(global_memory - global_memory_base));
    }
    printf("\n");
    printf("bits 16");
    printf("\n");
  }

  cmd_t *cmd = last_cmd->first;
  while (cmd != NULL) {
    if (!simulate) {
      size_t ip = cmd->start - decoder.start;
      for (size_t label_index = 0; label_index < decoder.label_count;
           label_index += 1) {
        label_t label = decoder.labels[label_index];
        if (label.ip == ip) {
          printf("%.*s:\n", STRING_FMT(label.label));
          break;
        }
      }

      if (verbose) {
        size_t len = cmd->end - cmd->start;
        printf("0x%03x  ", (uint32_t)ip);
        for (size_t i = 0; i < 6; i += 1) {
          if (i < len) {
            printf("%02x ", *(cmd->start + i));
          } else {
            printf("   ");
          }
        }
      }
    }

    printf("%.*s\n", STRING_FMT(cmd->text));
    cmd = cmd->next;
  }

  if (simulate) {
    printf("\nFinal registers:\n");
    for (u8 reg_idx = REGISTER_AX; reg_idx < REGISTER_COUNT; reg_idx += 1) {
      register_t reg = (register_t)reg_idx;
      register_map_t map = register_map[reg];
      if (map.mask != 0xFFFF) continue;

      u16 value = register_get(&sim, reg);
      if (value) {
        printf("      %.*s: 0x%04x (%d)\n", STRING_FMT(register_names[reg]), value, value);
      }
    }
    string_t flags = print_flags(sim.flags);
    printf("   flags: %.*s\n\n", STRING_FMT(flags));
  }

  return decoder.error.length;
}
