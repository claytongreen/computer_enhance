#include "sim.h"
#include "instruction.h"
#include "string.h"


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
    data = -((data ^ 0xFF) + 1);
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
    result.address.register_count = 2;
    if (mod) {
      result.address.offset = next_data16(instruction_stream, mod == 2 ? 1 : 0, 0);
    }
  }

  return result;
}

static instruction_t instruction_decode(simulator_t *sim, u32 offset) {
  string_t instruction_stream = { 0 };
  instruction_stream.data = sim->instruction_stream.data + offset;
  instruction_stream.length = sim->instruction_stream.length - offset;

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
      sim->error = string_pushf("\n!!! Unexpected opcode 0x%x\n", b1);
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
      sim->error = string_pushf("\n!!! Unexpected opcode 0x%02x\n", b1);
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
      sim->error = string_pushf("\n!!! Unexpected dest 0x%02x\n", b1);
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

    s8 label_index = -1;

    size_t label_ip = (instruction_stream.data - sim->instruction_stream.data) + inc;
    if (sim->label_count < LABEL_COUNT_MAX) { // have space for more labels
      // look for existing label
      for (size_t i = 0; i < sim->label_count; i += 1) {
        if (sim->labels[i].ip == label_ip) {
          label_index = i;
          break;
        }
      }

      if (label_index == -1) {
        string_t label = string_pushf("label%d", label_ip);
        label_index = sim->label_count++;
        sim->labels[label_index] = {label_ip, label};
      }
    } else {
      sim->error = STRING_LIT("Too many labels");
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
      sim->error = string_pushf("\n!!! Unexpected opcode 0x%02x\n", b1);
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
      sim->error = STRING_LIT("Invalid op: 1");
      break;
    case 2:
      sim->error = STRING_LIT("TODO: adc");
      break;
    case 3:
      sim->error = STRING_LIT("TODO: sbb");
      break;
    case 4:
      sim->error = STRING_LIT("Invalid op: 4");
      break;
    case 5:
      result.opcode = OP_CODE_SUB;
      break;
    case 6:
      sim->error = STRING_LIT("Invalid op: 6");
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

  case 0xC6: // MOV b,i,r/m
  case 0xC7: // MOV w,i,r/m
  {
    w = (b1 >> 0) & 1;

    u8 b2 = next_byte(&instruction_stream);
    u8 mod = (b2 >> 6);
    // TODO: op?
    u8 rm = (b2 >> 0) & 7;

    operand_t op_addr = next_address(&instruction_stream, w, mod, rm);
    u16 data = next_data16(&instruction_stream, w, 0);

    result.opcode = OP_CODE_MOV;

    result.dest = op_addr;

    result.source.kind = OPERAND_KIND_IMMEDIATE;
    result.source.immediate = data;

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
      sim->error = string_pushf("Unexpected op: 0b%.*s", 3, &bit_string_u8(op)[6]);
    }
  } break;

  default:
    sim->error = string_pushf("Unexpected instruction 0b%s", bit_string_u8(b1));
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
  if (value) *flags |=  flag;
  else       *flags &= ~flag;
}

static void set_flags(simulator_t *sim, instruction_t instruction, u16 result, u16 before) {
  flag_set_t flags = instruction_flags[instruction.opcode];

  for (u32 i = 0; i < flags.flag_count; i += 1) {
    flag_set_kind_t kind = flags.kinds[i];
    flag_t flag = flags.flags[i];

    b32 flag_result = kind;
    if (kind == FLAG_SET_KIND_RESULT) {
      flag_result = 0;

      if (flag & FLAG_CARRY) {
        // TODO
        b32 carry = 0;
        // if (instruction.flags & INSTRUCTION_FLAG_WIDE) {
          b32 a = (before >> 15) & 1;
          b32 b = (result >> 15) & 1;
          carry = a && !b;
        // } else {
        //   b32 a = ((before & 0xff) >> 7) & 1;
        //   b32 b = ((result & 0xff) >> 7) & 1;
        //   carry = a && !b;
        // }

        flag_result = carry;
      } else if (flag & FLAG_PARITY) {
        b32 parity = 1;
        s8 check = (s8)(result & 0xFF);
        for (u8 i = 0; i < 8; i += 1) {
          if (result & (1 << i)) {
            parity = parity ? 0 : 1;
          }
        }

        flag_result = parity;
      } else if (flag & FLAG_AUXILIARY_CARRY) {
        b32 aux = 0;

        // TODO: not quite right...
#define HI(x) (((x) >> 8) & 1)
#define LO(x) (((x) >> 7) & 1)
        aux = (LO(before) && HI(result)) || (HI(before) && LO(result));
#undef HI
#undef LO

        flag_result = aux;
      } else if (flag & FLAG_ZERO) {
        b32 zero = result == 0;
        flag_result = zero;
      } else if (flag & FLAG_SIGN) {
        b32 sign = 0x8000 & result;
        flag_result = sign;
      } else if (flag & FLAG_OVERFLOW) {
        // TODO: not quite right...
        b32 overflow = ((before >> 15) & 1) != ((result >> 15) & 1);
        flag_result = overflow;
      }
    }

    flag_set(&sim->flags, flag, flag_result);
  }
}

// NOTE(cg): comments showing current regsiter state always reference full 16bit register, never high/low portion
// TODO: return next IP?
static void instruction_simulate(simulator_t *sim, instruction_t instruction) {
  u16 result = 0;
  u16 before = 0;

  switch (instruction.opcode) {
  case OP_CODE_MOV: {
    register_t reg = instruction.dest.reg;

    // MOV ax, 1
    if (instruction.source.kind == OPERAND_KIND_IMMEDIATE) {
      result = instruction.source.immediate;
      register_set(sim, reg, result);
    }
    // MOV ax, bx
    else if (instruction.source.kind == OPERAND_KIND_REGISTER) {
      result = register_get(sim, instruction.source.reg);
      register_set(sim, reg, result);
    } else {
      sim->error = STRING_LIT("ERROR: simulate: Unhandled MOV");
      break;
    }
  } break;

  case OP_CODE_ADD:
  case OP_CODE_SUB: {
    register_t reg = instruction.dest.reg;

    s16 src_value = 0;

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

    before = register_get(sim, reg);
    result = before;
    if (instruction.opcode == OP_CODE_ADD) {
      result += src_value;
    } else {
      result -= src_value;
    }

    register_set(sim, reg, result);
  } break;

  case OP_CODE_CMP: {
    s16 src_value = 0;

    if (instruction.source.kind == OPERAND_KIND_REGISTER) {
      src_value = register_get(sim, instruction.source.reg);
    } else {
      sim->error = STRING_LIT("ERROR: simulate: unhandled CMP");
      break;
    }

    register_t dest = instruction.dest.reg;
    before = register_get(sim, dest);
    result = before - src_value;
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

  set_flags(sim, instruction, result, before);
}
