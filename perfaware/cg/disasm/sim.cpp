#include "sim.h"
#include "instruction.h"
#include "string.h"

// #include <string.h> // memset

static u32 sim_error_buffer_size = 4096;

#define SIM_ERROR(s, x, ...) do { \
  if (!(s)->error.data) { \
    (s)->error.data = (u8 *)malloc(sim_error_buffer_size); \
  } else { \
    memset((s)->error.data, 0, sim_error_buffer_size); \
  } \
  arena_temp_t temp = arena_temp_begin((s)->arena); \
  string_t file = STRING_LIT(__FILE__); \
  string_list_t parts = string_split((s)->arena, file, '\\'); \
  (s)->error.length = sprintf((char * const)(s)->error.data, "ERROR: %.*s:%d: " x, STRING_FMT(parts.last->string), __LINE__, __VA_ARGS__); \
  arena_temp_end(temp); \
} while(0)
  // fprintf(stderr, "%.*s\n", STRING_FMT((s)->error)); \

enum flag_set_kind_t {
  FLAG_SET_KIND_ZERO = 0,
  FLAG_SET_KIND_ONE  = 1,
  FLAG_SET_KIND_RESULT,
};
struct flag_set_t {
  u8 flag_count;

  flag_t          flags[6];
  flag_set_kind_t kinds[6];
};

static flag_set_t instruction_flags[OP_CODE_COUNT] = {
  {}, // OP_CODE_NONE
  // OP_CODE_ADD
  { 6,
    { FLAG_CARRY,           FLAG_ZERO,            FLAG_SIGN,            FLAG_OVERFLOW,        FLAG_PARITY,          FLAG_AUXILIARY_CARRY },
    { FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT },
  },
  // OP_CODE_CMP
  { 6,
    { FLAG_CARRY,           FLAG_ZERO,            FLAG_SIGN,            FLAG_OVERFLOW,        FLAG_PARITY,          FLAG_AUXILIARY_CARRY },
    { FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT },
  },
  // OP_CODE_DEC
  {},
  // OP_CODE_INC
  {},
  // OP_CODE_JA
  {},
  // OP_CODE_JB
  {},
  // OP_CODE_JBE
  {},
  // OP_CODE_JCXZ
  {},
  // OP_CODE_JE
  {},
  // OP_CODE_JG
  {},
  // OP_CODE_JL
  {},
  // OP_CODE_JLE
  {},
  // OP_CODE_JNB
  {},
  // OP_CODE_JNL
  {},
  // OP_CODE_JNO
  {},
  // OP_CODE_JNP
  {},
  // OP_CODE_JNS
  {},
  // OP_CODE_JNZ
  {},
  // OP_CODE_JO
  {},
  // OP_CODE_JP
  {},
  // OP_CODE_JS
  {},
  // OP_CODE_LOOP
  {},
  // OP_CODE_LOOPNZ
  {},
  // OP_CODE_LOOPZ
  {},
  // OP_CODE_MOV
  {},
  // OP_CODE_XCHG
  {},
  // OP_CODE_POP
  {},
  // OP_CODE_PUSH
  {},
  // OP_CODE_SUB
  { 6,
    { FLAG_CARRY,           FLAG_ZERO,            FLAG_SIGN,            FLAG_OVERFLOW,        FLAG_PARITY,          FLAG_AUXILIARY_CARRY },
    { FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT, FLAG_SET_KIND_RESULT },
  },
};

// TODO: there's got to be a better way
static register_t register_remap[REGISTER_COUNT] = {
    REGISTER_NONE,

    REGISTER_AX,   REGISTER_AX, REGISTER_AX, REGISTER_BX,
    REGISTER_BX,   REGISTER_BX, REGISTER_CX, REGISTER_CX,
    REGISTER_CX,   REGISTER_DX, REGISTER_DX, REGISTER_DX,

    REGISTER_SP,   REGISTER_BP, REGISTER_SI, REGISTER_DI,

    REGISTER_ES,   REGISTER_CS, REGISTER_SS, REGISTER_DS,
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

static void register_set(simulator_t *sim, register_t reg, u16 value) {
  register_map_t map = register_map[reg];
  assert(map.index != -1 && "TODO: handle register_set");

  if (map.memory_pointer) {
    u8 *p = sim->memory + map.index;
    u16 *p16 = (u16 *)p;
    *p16 = value;
  } else {
    sim->registers[map.index] = (sim->registers[map.index] & ~map.mask) | ((value << map.shift) & map.mask);
  }
}

static u16 address_get(simulator_t *sim, address_t addr) {
  u8 *p = sim->memory;
  for (u32 i = 0; i < addr.register_count; i += 1) {
    register_t reg = addr.registers[i];
    p += register_get(sim, reg);
  }
  p += addr.offset;

  return *p;
}

static void address_set(simulator_t *sim, address_t addr, u16 value) {
  u8 *p = sim->memory;
  for (u32 i = 0; i < addr.register_count; i += 1) {
    register_t reg = addr.registers[i];
    p += register_get(sim, reg);
  }
  p += addr.offset;

  *p = value;
}

static u8 next_byte(string_t *instruction_stream) {
  assert(instruction_stream->length > 0 && "Unexpected end of instruction stream");

  u8 byte = *(instruction_stream->data);
  instruction_stream->data += 1;
  instruction_stream->length -= 1;

  return byte;
}

static u16 next_data16(string_t *instruction_stream, u8 w, u8 s) {
  u16 data = next_byte(instruction_stream);

  if (s && w) {
    if (data & 0x80) {
      data |= 0xff00;
    }
  } else if (w) {
    u8 hi = next_byte(instruction_stream);
    data = (((s16)hi) << 8) | (data & 0xff);
  }

  return data;
}

static operand_t next_address(string_t *instruction_stream, u8 w, u8 mod, u8 rm) {
  operand_t result = {};

  if (mod == 3) {
    result.kind = OPERAND_KIND_REGISTER;
    result.reg = get_register(rm, w);
  } else if (mod == 0 && rm == 6) {
    result.kind = OPERAND_KIND_ADDRESS;
    result.address.offset = next_data16(instruction_stream, 1, 0);
  } else {
    result.kind = OPERAND_KIND_ADDRESS;
    result.address.registers[0] = effective_address[0][rm];
    result.address.registers[1] = effective_address[1][rm];
    if (result.address.registers[1] != REGISTER_NONE) {
      result.address.register_count = 2;
    } else {
      result.address.register_count = 1;
    }
    if (mod) {
      result.address.offset = next_data16(instruction_stream, mod == 2 ? 1 : 0, 0);
    }
  }

  return result;
}

static u32 ea_estimate_clocks(address_t addr) {
  u32 result = 0;

  if (addr.register_count == 2) {
    if (
      (addr.registers[0] == REGISTER_BP && addr.registers[1] == REGISTER_DI) ||
      (addr.registers[0] == REGISTER_BX && addr.registers[1] == REGISTER_SI)
    ) {
      result = 7;
    } else if (
      (addr.registers[0] == REGISTER_BP && addr.registers[1] == REGISTER_SI) ||
      (addr.registers[0] == REGISTER_BX && addr.registers[1] == REGISTER_DI)
    ) {
      result = 8;
    }
  } else if (addr.register_count == 1) {
    result = 5;
  }

  if (addr.offset) {
    if (!result) {
      result = 2;
    }

    result += 4;
  }

  return result;
}

static u32 instruction_estimate_clocks(simulator_t *sim, instruction_t instruction) {
  u32 result = 0;

  op_code_t opcode = instruction.opcode;
  operand_t dest = instruction.dest;
  operand_t source = instruction.source;

  switch (opcode) {
    case OP_CODE_MOV: {
      if (dest.kind == OPERAND_KIND_ADDRESS && source.kind == OPERAND_KIND_REGISTER) {
        if (source.reg == REGISTER_AX || source.reg == REGISTER_AH || source.reg == REGISTER_AL) {
          result = 10;
        } else {
          result = 9;
          result += ea_estimate_clocks(dest.address);
        }
      } else if (dest.kind == OPERAND_KIND_REGISTER && source.kind == OPERAND_KIND_ADDRESS) {
        if (dest.reg == REGISTER_AX || dest.reg == REGISTER_AH || dest.reg == REGISTER_AL) {
          result = 10;
        } else {
          result = 8;
          result += ea_estimate_clocks(source.address);
        }
      } else if (dest.kind == OPERAND_KIND_REGISTER && source.kind == OPERAND_KIND_REGISTER) {
        result = 2;
      } else if (dest.kind == OPERAND_KIND_REGISTER && source.kind == OPERAND_KIND_IMMEDIATE) {
        result = 4;
      } else {
        SIM_ERROR(sim, "Unexpected MOV permutation");
      }

    } break;

    case OP_CODE_ADD: {
      if (dest.kind == OPERAND_KIND_REGISTER && source.kind == OPERAND_KIND_REGISTER) {
        result = 3;
      } else if (dest.kind == OPERAND_KIND_REGISTER && source.kind == OPERAND_KIND_ADDRESS) {
        result = 9;
        result += ea_estimate_clocks(source.address);
        if (source.address.offset && source.address.offset % 2 != 0) {
          result += 4;
        }
      } else if (dest.kind == OPERAND_KIND_ADDRESS && source.kind == OPERAND_KIND_REGISTER) {
        result = 16;
        result += ea_estimate_clocks(dest.address);
        if (dest.address.offset && dest.address.offset % 2 != 0) {
          result += 8;
        }
      } else if (dest.kind == OPERAND_KIND_REGISTER && source.kind == OPERAND_KIND_IMMEDIATE) {
        result = 4;
      } else if (dest.kind == OPERAND_KIND_ADDRESS && source.kind == OPERAND_KIND_IMMEDIATE) {
        result = 17;
        result += ea_estimate_clocks(dest.address);
        if (dest.address.offset && dest.address.offset % 2 != 0) {
          result += 8;
        }
      } else {
        SIM_ERROR(sim, "Unexpected ADD permutation");
      }

    } break;

    default: {
      SIM_ERROR(sim, "Unexpected opcode: %.*s", STRING_FMT(op_code_names[opcode]));
    } break;
  }

  return result;
}

static instruction_t instruction_decode(simulator_t *sim, u32 offset) {
  string_t instruction_stream = { 0 };
  instruction_stream.data = sim->memory + offset;
  instruction_stream.length = sim->code_end - offset;

  instruction_t result = {};
  result.ip = instruction_stream.data;

  u8 b1 = next_byte(&instruction_stream);

  u8 w = 0;
  u8 s = 0;

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
    w = (b1 >> 0) & 1;

    u8 b2 = next_byte(&instruction_stream);
    u8 mod = (b2 >> 6);
    u8 reg = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    operand_t op_address = next_address(&instruction_stream, w, mod, rm);

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
      SIM_ERROR(sim, "Unexpected opcode 0x%x", b1);
      break;
    }

    operand_t op_register = {OPERAND_KIND_REGISTER};
    op_register.reg = get_register(reg, w);

    result.dest = d ? op_register : op_address;
    result.source = d ? op_address : op_register;

    if (result.dest.kind == OPERAND_KIND_ADDRESS) {
      result.flags |= INSTRUCTION_FLAG_SPECIFY_SIZE;
    }

  } break;

  case 0x04: // ADD b,ia
  case 0x05: // ADD w,ia
  case 0x2C: // SUB b,ia
  case 0x2D: // SUB w,ia
  case 0x3C: // CMP b,ia
  case 0x3D: // CMP w,ia
  {
    w = (b1 >> 0) & 1;

    u16 data = next_data16(&instruction_stream, w, 0);

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
      SIM_ERROR(sim, "Unexpected opcode 0x%02x", b1);
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
      SIM_ERROR(sim, "Unexpected dest 0x%02x", b1);
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
    u8 b2 = next_byte(&instruction_stream);
    u8 mod = (b2 >> 6);
    // u8 reg = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    operand_t address = next_address(&instruction_stream, 0, mod, rm);

    result.opcode = OP_CODE_POP;
    result.dest = address; // TODO: WORD?
  } break;

  case 0x87: // XCHG w,r/m
  {
    w = (b1 >> 0) & 1;

    u8 b2 = next_byte(&instruction_stream);
    u8 mod = (b2 >> 6);
    u8 reg = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    operand_t address = next_address(&instruction_stream, w, mod, rm);

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
    s8 inc = (s8)next_byte(&instruction_stream);

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
      SIM_ERROR(sim, "Unexpected opcode 0x%02x", b1);
      break;
    }

    result.flags |= INSTRUCTION_FLAG_JUMP;

    result.dest.kind = OPERAND_KIND_IMMEDIATE;
    result.dest.immediate = inc;
  } break;

  case 0x80: // Immed b,r/m
  case 0x81: // Immed w,r/m
  case 0x82: // Immed b,r/m
  case 0x83: // Immed is,r/m
  {
    s = (b1 >> 1) & 1;
    w = (b1 >> 0) & 1;

    u8 b2 = next_byte(&instruction_stream);
    u8 mod = (b2 >> 6);
    u8 op = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    operand_t dest = next_address(&instruction_stream, w, mod, rm);

    u16 data = next_data16(&instruction_stream, w, s);

    switch (op) {
    case 0:
      result.opcode = OP_CODE_ADD;
      break;
    case 1:
      SIM_ERROR(sim, "Invalid op: 1");
      break;
    case 2:
      SIM_ERROR(sim, "TODO: adc");
      break;
    case 3:
      SIM_ERROR(sim, "TODO: sbb");
      break;
    case 4:
      SIM_ERROR(sim, "Invalid op: 4");
      break;
    case 5:
      result.opcode = OP_CODE_SUB;
      break;
    case 6:
      SIM_ERROR(sim, "Invalid op: 6");
      break;
    case 7:
      result.opcode = OP_CODE_CMP;
      break;
    }

    result.source.kind = OPERAND_KIND_IMMEDIATE;
    result.source.immediate = data;

    result.dest = dest;

  } break;

  case 0x8C: // MOV sr,f,r/m
  {
    u8 b2 = next_byte(&instruction_stream);

    // sr: 00=ES, 01=CS, 10=SS, 11=DS

    u8 mod = (b2 >> 6) & 3;
    u8 sr = (b2 >> 3) & 3;
    u8 rm = (b2 >> 0) & 7;

    operand_t dest = next_address(&instruction_stream, 1, mod, rm);

    register_t reg = (register_t)(REGISTER_ES + sr);

    result.opcode = OP_CODE_MOV;
    result.dest = dest;
    result.source.kind = OPERAND_KIND_REGISTER;
    result.source.reg = reg;
  } break;
  case 0x8E: // MOV sr,t,r/m
  {
    u8 b2 = next_byte(&instruction_stream);

    // sr: 00=ES, 01=CS, 10=SS, 11=DS

    u8 mod = (b2 >> 6) & 3;
    u8 sr = (b2 >> 3) & 3;
    u8 rm = (b2 >> 0) & 7;

    operand_t source = next_address(&instruction_stream, 1, mod, rm);

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
    w = (b1 >> 0) & 1;

    operand_t op_addr = next_address(&instruction_stream, 0, 0, 6);

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
    w = b1 >= 0xB8; // TODO: does this "scale" (i.e. work for all values? probably not)

    u16 data = next_data16(&instruction_stream, w, 0);

    operand_t dest = {OPERAND_KIND_REGISTER};
    // dest.reg = (register_t)(b1 - 0xB0 + 1);
    dest.reg = get_register((b1 - 0xB0) % 8, w);

    operand_t source = {OPERAND_KIND_IMMEDIATE};
    source.immediate = data;

    result.opcode = OP_CODE_MOV;
    result.dest = dest;
    result.source = source;
  } break;

  case 0xCC: // INT 3
  {
    result.opcode = OP_CODE_INT;
    result.dest.kind = OPERAND_KIND_IMMEDIATE;
    result.dest.immediate = 3;
  } break;

  case 0xC6: // MOV b,i,r/m
  case 0xC7: // MOV w,i,r/m
  {
    w = (b1 >> 0) & 1;

    u8 b2 = next_byte(&instruction_stream);
    u8 mod = (b2 >> 6);
    // TODO: op?
    u8 rm = (b2 >> 0) & 7;

    operand_t dest = next_address(&instruction_stream, w, mod, rm);
    u16 data = next_data16(&instruction_stream, w, 0);

    result.opcode = OP_CODE_MOV;

    result.dest = dest;

    result.source.kind = OPERAND_KIND_IMMEDIATE;
    result.source.immediate = data;

    if (dest.kind == OPERAND_KIND_ADDRESS) {
      result.flags |= INSTRUCTION_FLAG_SPECIFY_SIZE;
    }
  } break;

  // INC, DEC, CALL id, CALL l id, JMP id, PUSH, -
  case 0xFF: // Grp 2 w,r/m
  {
    u8 b2 = next_byte(&instruction_stream);
    u8 mod = (b2 >> 6);
    u8 op = (b2 >> 3) & 7;
    u8 rm = (b2 >> 0) & 7;

    // PUSH MEM16
    if (op == 6) {
      result.opcode = OP_CODE_PUSH;

      operand_t address = next_address(&instruction_stream, 1, mod, rm);
      result.dest = address;
    }
    // INC  MEM16
    else if (op == 0) {
      result.opcode = OP_CODE_INC;
      operand_t address = next_address(&instruction_stream, 1, mod, rm);
      result.dest = address;
      // case 1: break; // DEC  MEM16
      // case 2: break; // CALL REG16/MEM16 (intra)
      // case 3: break; // CALL MEM16 (intersegment)
      // case 4: break; // JMP  REG16/MEM16 (intersegment)
      // case 7: assert(false); break; // (not used)
    } else {
      SIM_ERROR(sim, "Unexpected op: 0b%.*s", 3, &bit_string_u8(op)[6]);
    }
  } break;

  default:
    SIM_ERROR(sim, "Unexpected instruction 0x%02x 0b%s", b1, bit_string_u8(b1));
    break;
  }

  if (w) {
    result.flags |= INSTRUCTION_FLAG_WIDE;
  }
  if (s) {
    result.flags |= INSTRUCTION_FLAG_SIGN_EXTEND;
  }

  result.bytes_count = instruction_stream.data - result.ip;

  return result;
}

static void flag_set(u16 *flags, flag_t flag, b32 value) {
  if (value) *flags |=  (1 << flag);
  else       *flags &= ~(1 << flag);
}

static b32 flag_get(u16 flags, flag_t flag) {
  b32 result = (flags & (1 << flag));
  return result;
}

static void set_flags(simulator_t *sim, instruction_t instruction, u16 result, u16 dst, u16 src) {
  flag_set_t flags = instruction_flags[instruction.opcode];

  for (u32 i = 0; i < flags.flag_count; i += 1) {
    flag_set_kind_t kind = flags.kinds[i];
    flag_t flag = flags.flags[i];

    b32 flag_result = kind;
    if (kind == FLAG_SET_KIND_RESULT) {
      flag_result = 0;

      if (flag == FLAG_CARRY) {
        b32 carry = 0;

        if (instruction.opcode == OP_CODE_ADD) {
          carry = (dst & 0x8000) && !(result & 0x8000);
        } else if (instruction.opcode == OP_CODE_SUB || instruction.opcode == OP_CODE_CMP) {
          carry = !(dst & 0x8000) && (result & 0x8000);
        } else {
          SIM_ERROR(sim, "Unexpected opcode %.*s when setting CF", STRING_FMT(op_code_names[instruction.opcode]));
        }

        flag_result = carry;
      } else if (flag == FLAG_PARITY) {
        b32 parity = 1;
        s8 check = (s8)(result & 0xFF);
        for (u8 i = 0; i < 8; i += 1) {
          if (result & (1 << i)) {
            parity = parity ? 0 : 1;
          }
        }

        flag_result = parity;
      } else if (flag == FLAG_AUXILIARY_CARRY) {
        b32 aux = 0;

        switch (instruction.opcode) {
        case OP_CODE_ADD:
        case OP_CODE_INC:
          aux = ((dst & 0xF) + (src & 0xF)) > 0xf;
          break;

        case OP_CODE_SUB:
        case OP_CODE_CMP:
        case OP_CODE_DEC:
          aux = ((dst & 0xF) < (src & 0xF));
          break;

        case OP_CODE_NONE:
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
        case OP_CODE_MOV:
        case OP_CODE_XCHG:
        case OP_CODE_POP:
        case OP_CODE_PUSH:
        case OP_CODE_COUNT:
          SIM_ERROR(sim, "Unexpected opcode \"%.*s\" when setting AF", STRING_FMT(op_code_names[instruction.opcode]));
          break;
        }

        flag_result = aux;
      } else if (flag == FLAG_ZERO) {
        b32 zero = (result == 0);
        flag_result = zero;
      } else if (flag == FLAG_SIGN) {
        b32 sign = (0x8000 & result);
        flag_result = sign;
      } else if (flag == FLAG_OVERFLOW) {
        b32 overflow = (dst & 0x8000) != (result & 0x8000);
        flag_result = overflow;
      }
    }

    flag_set(&sim->flags, flag, flag_result);
  }
}

// NOTE(cg): comments showing current regsiter state always reference full 16bit register, never high/low portion
// TODO: return next IP?
static void instruction_simulate(simulator_t *sim, instruction_t instruction) {
  if (sim->error.length) return;

  u32 ip = sim->ip + instruction.bytes_count;

  u16 dst_val = 0;
  u16 src_val = 0;
  u16 res_val = 0;

  switch (instruction.opcode) {
  case OP_CODE_MOV: {

    if (instruction.dest.kind == OPERAND_KIND_REGISTER) {
      register_t reg = instruction.dest.reg;
      dst_val = register_get(sim, reg);

      // MOV ax, 1
      if (instruction.source.kind == OPERAND_KIND_IMMEDIATE) {
        res_val = instruction.source.immediate;
      }
      // MOV ax, bx
      else if (instruction.source.kind == OPERAND_KIND_REGISTER) {
        res_val = register_get(sim, instruction.source.reg);
      }
      // MOV ax, [1000]
      else if (instruction.source.kind == OPERAND_KIND_ADDRESS) {
        address_t addr = instruction.source.address;
        res_val = address_get(sim, addr);
      } else {
        SIM_ERROR(sim, "simulate, Unhandled MOV source");
        break;
      }

      register_set(sim, reg, res_val);

    } else if (instruction.dest.kind == OPERAND_KIND_ADDRESS) {
      address_t addr = instruction.dest.address;

      // MOV [1000], 1
      if (instruction.source.kind == OPERAND_KIND_IMMEDIATE) {
        res_val = instruction.source.immediate;
      }
      // MOV [1000], bx
      else if (instruction.source.kind == OPERAND_KIND_REGISTER) {
        res_val = register_get(sim, instruction.source.reg);
      }
      // MOV [1000], [1001]
      else if (instruction.source.kind == OPERAND_KIND_ADDRESS) {
        // TODO: POSSIBLE???
        address_t addr = instruction.source.address;
        res_val = address_get(sim, addr);
      } else {
        SIM_ERROR(sim, "simulate, Unhandled MOV source");
        break;
      }

      address_set(sim, addr, res_val);
    } else {
      SIM_ERROR(sim, "simulate, unhandled MOV dest");
      break;
    }
  } break;

  case OP_CODE_ADD:
  case OP_CODE_SUB: {
    register_t reg = instruction.dest.reg;

    // ADD/SUB ax, 1
    if (instruction.source.kind == OPERAND_KIND_IMMEDIATE) {
      src_val = instruction.source.immediate;
    }
    // ADD/SUB ax, bx
    else if (instruction.source.kind == OPERAND_KIND_REGISTER) {
      src_val = register_get(sim, instruction.source.reg);
    }
    // ADD/SUB ax, [1000]
    else if (instruction.source.kind == OPERAND_KIND_ADDRESS) {
      address_t addr = instruction.source.address;
      src_val = address_get(sim, addr);
    }
    else {
      SIM_ERROR(sim, "ERROR: simulate: unhandled SUB");
      break;
    }

    dst_val = register_get(sim, reg);
    res_val = dst_val;
    if (instruction.opcode == OP_CODE_ADD) {
      res_val += src_val;
    } else {
      res_val -= src_val;
    }

    register_set(sim, reg, res_val);
  } break;

  case OP_CODE_CMP: {
    s16 src_value = 0;

    if (instruction.source.kind == OPERAND_KIND_REGISTER) {
      src_val = register_get(sim, instruction.source.reg);
    } else if (instruction.source.kind == OPERAND_KIND_IMMEDIATE) {
      src_val = instruction.source.immediate;
    } else {
      SIM_ERROR(sim, "ERROR: simulate: unhandled CMP");
      break;
    }

    register_t dest = instruction.dest.reg;
    dst_val = register_get(sim, dest);
    res_val = dst_val - src_val;
  } break;

  case OP_CODE_LOOP: {
    u16 cx = register_get(sim, REGISTER_CX);
    cx -= 1;
    register_set(sim, REGISTER_CX, cx);
    if (cx != 0) {
      s8 inc = (s8)instruction.dest.immediate;
      ip += inc;
    }
  } break;

  case OP_CODE_LOOPZ: {
    u16 cx = register_get(sim, REGISTER_CX);
    cx -= 1;
    register_set(sim, REGISTER_CX, cx);
    if (cx == 0) {
        break;
    } else {
      // FALLTHROUGH
    }
  }
  case OP_CODE_JE: {
    if (flag_get(sim->flags, FLAG_ZERO)) {
      s8 inc = (s8)instruction.dest.immediate;
      ip += inc;
    }
  } break;
  case OP_CODE_LOOPNZ: {
    u16 cx = register_get(sim, REGISTER_CX);
    cx -= 1;
    register_set(sim, REGISTER_CX, cx);
    if (cx == 0) {
      break;
    } else {
      // FALLTHROUGH
    }
  }
  case OP_CODE_JNZ: {
    if (!flag_get(sim->flags, FLAG_ZERO)) {
      s8 inc = (s8)instruction.dest.immediate;
      ip += inc;
    }
  } break;

  case OP_CODE_JP: {
    if (flag_get(sim->flags, FLAG_PARITY)) {
      s8 inc = (s8)instruction.dest.immediate;
      ip += inc;
    }
  } break;
  case OP_CODE_JNP: {
    if (!flag_get(sim->flags, FLAG_PARITY)) {
      s8 inc = (s8)instruction.dest.immediate;
      ip += inc;
    }
  } break;

  case OP_CODE_JB: {
    if (flag_get(sim->flags, FLAG_CARRY)) {
      s8 inc = (s8)instruction.dest.immediate;
      ip += inc;
    }
  } break;

  case OP_CODE_NONE: { } break;

  case OP_CODE_DEC:
  case OP_CODE_INC:
  case OP_CODE_JA:
  case OP_CODE_JBE:
  case OP_CODE_JCXZ:
  case OP_CODE_JG:
  case OP_CODE_JL:
  case OP_CODE_JLE:
  case OP_CODE_JNB:
  case OP_CODE_JNL:
  case OP_CODE_JNO:
  case OP_CODE_JNS:
  case OP_CODE_JO:
  case OP_CODE_JS:
  case OP_CODE_XCHG:
  case OP_CODE_POP:
  case OP_CODE_PUSH:
  case OP_CODE_COUNT: {
    SIM_ERROR(sim, "ERROR: simulate: unhandled opcode: %.*s", STRING_FMT(op_code_names[instruction.opcode]));
  } break;
  }

  set_flags(sim, instruction, res_val, dst_val, src_val);

  sim->ip = ip;
}

void sim_load(simulator_t *sim, string_t obj) {
  // "load" the program into memory
  memcpy(sim->memory, obj.data, obj.length);

  // TODO: this should be probably somewhere else...
  // NOTE(cg): stick an "int 3" at the end of the code
  sim->memory[obj.length + 0] = 0xcc;
  sim->memory[obj.length + 1] = 0xcc;
  sim->memory[obj.length + 2] = 0xcc;

  sim->code_end = obj.length + 2;
}

instruction_t sim_step(simulator_t *sim) {
  instruction_t instruction = instruction_decode(sim, sim->ip);
  instruction_simulate(sim, instruction);

  return instruction;
}

void sim_reset(simulator_t *sim) {
  sim->ip = 0;
  sim->error.length = 0;

  arena_reset(sim->arena);

  memset(sim->memory + sim->code_end,  0, MB(1) - sim->code_end);
  memset(sim->registers,               0, ARRAY_COUNT(sim->registers));

  sim->flags = 0;
}


