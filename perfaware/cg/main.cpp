#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <intrin.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#define assert(Cond) do { if (!(Cond)) __debugbreak(); } while (0)

typedef uint8_t   u8;
typedef int8_t    s8;
typedef uint16_t u16;
typedef int16_t  s16;
typedef uint32_t u32;
typedef int32_t  s32;

typedef int32_t  b32;

// MEMORY ---------------------------------------------------------------------
static size_t global_memory_size = 4096 * 8;
static u8* global_memory_base;
static u8* global_memory;
#define PUSH_ARRAY(TYPE, COUNT) (TYPE *)global_memory;                                            \
  memset(global_memory, 0, sizeof(TYPE) * (COUNT));                                               \
  global_memory += sizeof(TYPE) * (COUNT);                                                        \
  assert(((size_t)(global_memory - global_memory_base) < global_memory_size) && "OUT OF MEMORY")
// MEMORY ---------------------------------------------------------------------

#include "string.cpp"

// OS -------------------------------------------------------------------------
static string_t read_entire_file(char* filename) {
  string_t result = {};

  FILE* file = fopen(filename, "rb");
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
  string_t instruction_stream;
  string_t error;

  u8 *start;

  label_t labels[LABEL_COUNT_MAX];
  u32 label_count;
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
  REGISTER_AL,
  REGISTER_CL,
  REGISTER_DL,
  REGISTER_BL,
  REGISTER_AH,
  REGISTER_CH,
  REGISTER_DH,
  REGISTER_BH,
  REGISTER_AX,
  REGISTER_CX,
  REGISTER_DX,
  REGISTER_BX,
  REGISTER_SP,
  REGISTER_BP,
  REGISTER_SI,
  REGISTER_DI,
  REGISTER_CS,
  REGISTER_DS,
  // --------------
  REGISTER_COUNT,
};

static string_t register_names[REGISTER_COUNT] = {
  str_none,
  STRING_LIT("al"),
  STRING_LIT("cl"),
  STRING_LIT("dl"),
  STRING_LIT("bl"),
  STRING_LIT("ah"),
  STRING_LIT("ch"),
  STRING_LIT("dh"),
  STRING_LIT("bh"),
  STRING_LIT("ax"),
  STRING_LIT("cx"),
  STRING_LIT("dx"),
  STRING_LIT("bx"),
  STRING_LIT("sp"),
  STRING_LIT("bp"),
  STRING_LIT("si"),
  STRING_LIT("di"),
  STRING_LIT("cs"),
  STRING_LIT("ds"),
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
  u8  bytes_count;

  operand_t dest;
  operand_t source;

  u8 flags; // ???
};

static register_t get_register(u8 reg, u8 w) {
  register_t result = (register_t)((w * 8) + (reg + 1));
  return result;
}

static string_t get_register_name(u8 reg, u8 w) {
  register_t r = get_register(reg, w);
  return register_names[r];
}

static u8 next_byte(string_t* instruction_stream) {
  assert(instruction_stream->length > 0 && "Unexpected end of instruction stream");

  u8 byte = *(instruction_stream->data);
  instruction_stream->data   += 1;
  instruction_stream->length -= 1;

  return byte;
}

static s16 next_data_s16(string_t *instruction_stream) {
  u8 data = (s16)next_byte(instruction_stream);
  return data;
}

static u16 next_data_u16(string_t* instruction_stream, u8 w) {
  u8 lo = next_byte(instruction_stream);
  u8 hi = (w == 1) ? next_byte(instruction_stream) : ((lo & 0x80) ? 0xff : 0);

  u16 data = (((s16)hi) << 8) | (u16)lo;

  return data;
}

static register_t effective_address_one[8] = { REGISTER_BX, REGISTER_BX, REGISTER_BP, REGISTER_BP, REGISTER_SI,   REGISTER_DI,   REGISTER_BP,   REGISTER_BX };
static register_t effective_address_two[8] = { REGISTER_SI, REGISTER_DI, REGISTER_SI, REGISTER_DI, REGISTER_NONE, REGISTER_NONE, REGISTER_NONE, REGISTER_NONE };
static operand_t next_address(string_t *instruction_stream, u8 w, u8 mod, u8 rm) {
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
    result.address.registers[0]   = effective_address_one[rm];
    result.address.registers[1]   = effective_address_two[rm];
    result.address.register_count = 2;
    if (mod) {
      // @TODO: next_data_s16?
      result.address.offset = next_data_u16(instruction_stream, mod == 2 ? 1 : 0);
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
      u8 rm  = (b2 >> 0) & 7;

      operand_t op_address = next_address(instruction_stream, w, mod, rm);

      switch (b1) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03: result.opcode = OP_CODE_ADD; break;

        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B: result.opcode = OP_CODE_SUB; break;

        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B: result.opcode = OP_CODE_CMP; break;

        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B: result.opcode = OP_CODE_MOV; break;

        default: 
          decoder->error = string_pushf("\n!!! Unexpected opcode 0x%x\n", b1);
          break;
      }

      operand_t op_register = { OPERAND_KIND_REGISTER };
      op_register.reg = get_register(reg, w);

      result.dest   = d ? op_register : op_address;
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
        case 0x05: result.opcode = OP_CODE_ADD; break;

        case 0x2C:
        case 0x2D: result.opcode = OP_CODE_SUB; break;

        case 0x3C:
        case 0x3D: result.opcode = OP_CODE_CMP; break;

        default: 
          decoder->error = string_pushf("\n!!! Unexpected opcode 0x%02x\n", b1);
          break;
      }
      operand_t dest = { OPERAND_KIND_REGISTER };
      switch (b1) {
        case 0x04:
        case 0x2C:
        case 0x3C: dest.reg = REGISTER_AL; break;

        case 0x05:
        case 0x2D:
        case 0x3D: dest.reg = REGISTER_AX; break;

        default: 
          decoder->error = string_pushf("\n!!! Unexpected dest 0x%02x\n", b1);
          break;
      }

      operand_t source = { OPERAND_KIND_IMMEDIATE };
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
      u8 rm  = (b2 >> 0) & 7;

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
      u8 rm  = (b2 >> 0) & 7;

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
          decoder->labels[label_index] = { label_ip, label };
        }
      } else {
        decoder->error = STRING_LIT("Too many labels");
      }

      // TODO: Is there a nice way to map this?
      switch(b1) {
        case 0x70: result.opcode = OP_CODE_JO;     break;
        case 0x71: result.opcode = OP_CODE_JNO;    break;
        case 0x72: result.opcode = OP_CODE_JB;     break;
        case 0x73: result.opcode = OP_CODE_JNB;    break;
        case 0x74: result.opcode = OP_CODE_JE;     break;
        case 0x75: result.opcode = OP_CODE_JNZ;    break;
        case 0x76: result.opcode = OP_CODE_JBE;    break;
        case 0x77: result.opcode = OP_CODE_JA;     break;
        case 0x78: result.opcode = OP_CODE_JS;     break;
        case 0x79: result.opcode = OP_CODE_JNS;    break;
        case 0x7A: result.opcode = OP_CODE_JP;     break;
        case 0x7B: result.opcode = OP_CODE_JNP;    break;
        case 0x7C: result.opcode = OP_CODE_JL;     break;
        case 0x7D: result.opcode = OP_CODE_JNL;    break;
        case 0x7E: result.opcode = OP_CODE_JLE;    break;
        case 0x7F: result.opcode = OP_CODE_JG;     break;
        case 0xE0: result.opcode = OP_CODE_LOOPNZ; break;
        case 0xE1: result.opcode = OP_CODE_LOOPZ;  break;
        case 0xE2: result.opcode = OP_CODE_LOOP;   break;
        case 0xE3: result.opcode = OP_CODE_JCXZ;   break;

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
      u8 op  = (b2 >> 3) & 7;
      u8 rm  = (b2 >> 0) & 7;

      operand_t dest = next_address(instruction_stream, w, mod, rm);

      u16 data = b1 == 0x83 ? next_data_s16(instruction_stream) : next_data_u16(instruction_stream, w);

      switch (op) {
        case 0:  result.opcode = OP_CODE_ADD; break;
        case 1: decoder->error = STRING_LIT("Invalid op: 1"); break;
        case 2: decoder->error = STRING_LIT("TODO: adc");     break;
        case 3: decoder->error = STRING_LIT("TODO: sbb");     break;
        case 4: decoder->error = STRING_LIT("Invalid op: 4"); break;
        case 5:  result.opcode = OP_CODE_SUB; break;
        case 6: decoder->error = STRING_LIT("Invalid op: 6"); break;
        case 7:  result.opcode = OP_CODE_CMP; break;
      }

      result.source.kind = (mod == 3) ? OPERAND_KIND_IMMEDIATE : (w ? OPERAND_KIND_IMMEDIATE16 : OPERAND_KIND_IMMEDIATE8);
      result.source.immediate = data;

      result.dest = dest;

    } break;

    case 0xA0: // MOV m -> AL
    case 0xA1: // MOV m -> AX
    case 0xA2: // MOV AX -> m
    case 0xA3: // MOV AX -> m
    {
      u8 d = (b1 >> 1) & 1;
      u8 w = (b1 >> 0) & 1;

      operand_t op_addr = next_address(instruction_stream, 0, 0, 6);

      operand_t op_reg = { OPERAND_KIND_REGISTER };
      op_reg.reg = w ? REGISTER_AX : REGISTER_AL;

      result.opcode = OP_CODE_MOV;
      result.dest   = d ? op_addr : op_reg;
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
      u8 w = b1 >= 0xB8; // TODO: does this "scale" (i.e. work for all values? probably not)

      u16 data = next_data_u16(instruction_stream, w);

      operand_t dest = { OPERAND_KIND_REGISTER };
      dest.reg = (register_t)(b1 - 0xB0 + 1);

      operand_t source = { OPERAND_KIND_IMMEDIATE };
      source.immediate = data;

      result.opcode =  OP_CODE_MOV;
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
      u8 rm  = (b2 >> 0) & 7;

      operand_t op_addr = next_address(instruction_stream, w, mod, rm);
      u16 data     = next_data_u16(instruction_stream, w);

      result.opcode = OP_CODE_MOV;

      result.dest   = op_addr;

      result.source.kind = w ? OPERAND_KIND_IMMEDIATE16 : OPERAND_KIND_IMMEDIATE8;
      result.source.immediate = data;

    } break;

    // INC, DEC, CALL id, CALL l id, JMP id, PUSH, -
    case 0xFF: // Grp 2 w,r/m
    {
      u8 b2 = next_byte(instruction_stream);
      u8 mod = (b2 >> 6);
      u8 op  = (b2 >> 3) & 7;
      u8 rm  = (b2 >> 0) & 7;

      switch (op) {
        case 6: // PUSH MEM16
          result.opcode = OP_CODE_PUSH;

          operand_t address = next_address(instruction_stream, 1, mod, rm);
          result.dest = address;
          break;

        // case 0: break; // INC  MEM16
        // case 1: break; // DEC  MEM16
        // case 2: break; // CALL REG16/MEM16 (intra)
        // case 3: break; // CALL MEM16 (intersegment)
        // case 4: break; // JMP  REG16/MEM16 (intersegment)
        // case 7: assert(false); break; // (not used)

        default: 
          decoder->error = string_pushf("Unexpected op: 0b%.*s", 3, &bit_string_u8(op)[6]);
          break;
      }
    } break;

    default:
      decoder->error = string_pushf("Unexpected instruction 0b%s", bit_string_u8(b1));
      break;
  }

  return result;
}

static string_t operand_print(decoder_t *decoder, operand_t operand) {
  string_t result = { 0 };

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

      string_list_t sb = { 0 };
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

    case OPERAND_KIND_NONE: break;
  }

  return result;
}

static string_t instruction_print(decoder_t *decoder, instruction_t instruction) {
  string_t result;

  string_t opcode = op_code_names[instruction.opcode];
  string_t dest = operand_print(decoder, instruction.dest);

  if (instruction.source.kind != OPERAND_KIND_NONE) {
    string_t source = operand_print(decoder, instruction.source);
    result = string_pushf("%.*s %.*s, %.*s", STRING_FMT(opcode), STRING_FMT(dest), STRING_FMT(source));
  } else {
    result = string_pushf("%.*s %.*s", STRING_FMT(opcode), STRING_FMT(dest));
  }

  return result;
}

static u16 registers[8];

static void register_set(register_t reg, s16 value) {
  switch (reg) {
  case REGISTER_AL: registers[0] &= value & 0xFF; break;
  case REGISTER_AH: registers[0] &= ((value & 0xFF) << 8); break;
  case REGISTER_AX: registers[0]  = value; break;

  case REGISTER_BL: registers[1] &= value & 0xFF; break;
  case REGISTER_BH: registers[1] &= ((value & 0xFF) << 8); break;
  case REGISTER_BX: registers[1]  = value; break;

  case REGISTER_CL: registers[2] &= value & 0xFF; break;
  case REGISTER_CH: registers[2] &= ((value & 0xFF) << 8); break;
  case REGISTER_CX: registers[2]  = value; break;

  case REGISTER_DL: registers[3] &= value & 0xFF; break;
  case REGISTER_DH: registers[3] &= ((value & 0xFF) << 8); break;
  case REGISTER_DX: registers[3]  = value; break;

  case REGISTER_SP: registers[4]  = value; break;
  case REGISTER_BP: registers[5]  = value; break;
  case REGISTER_SI: registers[6]  = value; break;
  case REGISTER_DI: registers[7]  = value; break;

  case REGISTER_CS:
  case REGISTER_DS:
  case REGISTER_NONE:
  case REGISTER_COUNT:
    // TODO: no bueno
    break;
  }
}

static s16 register_get(register_t reg) {
  s16 result = 0;

  switch (reg) {
  case REGISTER_AL: result = registers[0] & 0xFF; break;
  case REGISTER_AH: result = registers[0] & 0xFF00; break;
  case REGISTER_AX: result = registers[0]; break;

  case REGISTER_BL: result = registers[1] & 0xFF; break;
  case REGISTER_BH: result = registers[1] & 0xFF00; break;
  case REGISTER_BX: result = registers[1]; break;

  case REGISTER_CL: result = registers[2] & 0xFF; break;
  case REGISTER_CH: result = registers[2] & 0xFF00; break;
  case REGISTER_CX: result = registers[2]; break;

  case REGISTER_DL: result = registers[3] & 0xFF; break;
  case REGISTER_DH: result = registers[3] & 0xFF00; break;
  case REGISTER_DX: result = registers[3]; break;

  case REGISTER_SP: result = registers[4]; break;
  case REGISTER_BP: result = registers[5]; break;
  case REGISTER_SI: result = registers[6]; break;
  case REGISTER_DI: result = registers[7]; break;

  case REGISTER_CS:
  case REGISTER_DS:
  case REGISTER_NONE:
  case REGISTER_COUNT:
    break;
  }

  return result;
}

static string_t instruction_simulate(decoder_t *decoder, instruction_t instruction) {
  string_list_t sb = {};

  string_list_push(&sb, instruction_print(decoder, instruction));

  switch (instruction.opcode) {
  case OP_CODE_MOV: {
      if (instruction.dest.kind == OPERAND_KIND_REGISTER && instruction.source.kind == OPERAND_KIND_IMMEDIATE) {
        register_t reg = instruction.dest.reg;
        s16 before = register_get(reg);
        s16 after = instruction.source.immediate;
        register_set(reg, after);
        string_list_pushf(&sb, " ; %.*s:0x%d->0x%d", STRING_FMT(register_names[reg]), before, after);
      } else {
        decoder->error = STRING_LIT("ERROR: simulate: Unhandled MOV");
      }
  } break;

  case OP_CODE_NONE:
  case OP_CODE_ADD:
  case OP_CODE_CMP:
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
  case OP_CODE_SUB:
  case OP_CODE_COUNT: {
      decoder->error = string_pushf("ERROR: simulate: unhandled opcode: %.*s", op_code_names[instruction.opcode]);
  } break;
  }

  return string_list_join(&sb);
}

int main(int argc, char** argv) {
  if (argc == 1) {
    fprintf(stderr, "Usage: sim[filename]\n");
    return 1;
  }

  int print_bytes = argc > 2;
  b32 simulate = print_bytes;

  global_memory_base = (u8 *)malloc(global_memory_size);
  assert(global_memory_base && "Failed to allocate data");
  memset(global_memory_base, 0, global_memory_size);
  global_memory = global_memory_base;

  decoder_t decoder = { 0 };

  char* filename = argv[1];
  decoder.instruction_stream = read_entire_file(filename);
  assert(decoder.instruction_stream.length != 0 && "Failed to read file");

  decoder.start = decoder.instruction_stream.data;

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
    if (decoder.instruction_stream.length == 0) break;

    u8 *instruction_stream_start = decoder.instruction_stream.data;

    instruction_t instruction = instruction_decode(&decoder, 0);

    // TODO: handle "simulating" instructions

    string_t text;
    if (decoder.error.length) {
      text = string_pushf("??? ; %.*s", STRING_FMT(decoder.error));
    } else if (simulate) {
      text = instruction_simulate(&decoder, instruction);
    } else {
      text = instruction_print(&decoder, instruction);
    }

    // @TODO: this is only here because of labeled jmp's. But if we don't do those, we can just print directly.
    cmd_t *cmd = PUSH_ARRAY(cmd_t, 1);
    cmd->start = instruction_stream_start;
    cmd->end   = decoder.instruction_stream.data;
    cmd->text  = text;

    if (last_cmd) {
      cmd->prev = last_cmd;
      cmd->first = last_cmd->first;
      last_cmd->next = cmd;
    } else {
      cmd->first = cmd;
    }
    last_cmd = cmd;

    if (decoder.error.length) break;
  }

  // Print the stuff

  if (simulate) {
    printf("--- test\\%s execution ---\n", filename);
  } else {
    printf("; %s  %zd bytes\n", filename, (size_t)(global_memory - global_memory_base));
    printf("\n");
    if (!print_bytes) {
      printf("bits 16");
      printf("\n");
    }
  }

  cmd_t *cmd = last_cmd->first;
  while (cmd != NULL) {
    if (!simulate) {
      size_t ip = cmd->start - decoder.start;
      for (size_t label_index = 0; label_index < decoder.label_count; label_index += 1) {
        label_t label = decoder.labels[label_index];
        if (label.ip == ip) {
          printf("%.*s:\n", STRING_FMT(label.label));
          break;
        }
      }

      if (!simulate) {
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
    printf("      ax: 0x%04d (%d)\n", register_get(REGISTER_AX), register_get(REGISTER_AX));
    printf("      bx: 0x%04d (%d)\n", register_get(REGISTER_BX), register_get(REGISTER_BX));
    printf("      cx: 0x%04d (%d)\n", register_get(REGISTER_CX), register_get(REGISTER_CX));
    printf("      dx: 0x%04d (%d)\n", register_get(REGISTER_DX), register_get(REGISTER_DX));
    printf("      sp: 0x%04d (%d)\n", register_get(REGISTER_SP), register_get(REGISTER_SP));
    printf("      bp: 0x%04d (%d)\n", register_get(REGISTER_BP), register_get(REGISTER_BP));
    printf("      si: 0x%04d (%d)\n", register_get(REGISTER_SI), register_get(REGISTER_SI));
    printf("      di: 0x%04d (%d)\n", register_get(REGISTER_DI), register_get(REGISTER_DI));
    printf("\n");
  }

  return decoder.error.length;
}
