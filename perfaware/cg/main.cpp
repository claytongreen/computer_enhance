#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
//#include <malloc.h>
#include <intrin.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#define assert(Cond) do { if (!(Cond)) __debugbreak(); } while (0)

// MEMORY ---------------------------------------------------------------------
static size_t global_memory_size = 4096 * 8;
static uint8_t* global_memory_base;
static uint8_t* global_memory;
#define PUSH_ARRAY(TYPE, COUNT) (TYPE *)global_memory;                                            \
  memset(global_memory, 0, sizeof(TYPE) * (COUNT));                                               \
  global_memory += sizeof(TYPE) * (COUNT);                                                        \
  assert(((size_t)(global_memory - global_memory_base) < global_memory_size) && "OUT OF MEMORY")
// MEMORY ---------------------------------------------------------------------

#include "string.h"

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

enum operand_t {
  OPERAND_UNKNOWN,
  OPERAND_ADD,
  OPERAND_CMP,
  OPERAND_JA,
  OPERAND_JB,
  OPERAND_JBE,
  OPERAND_JCXZ,
  OPERAND_JE,
  OPERAND_JG,
  OPERAND_JL,
  OPERAND_JLE,
  OPERAND_JNB,
  OPERAND_JNL,
  OPERAND_JNO,
  OPERAND_JNP,
  OPERAND_JNS,
  OPERAND_JNZ,
  OPERAND_JO,
  OPERAND_JP,
  OPERAND_JS,
  OPERAND_LOOP,
  OPERAND_LOOPNZ,
  OPERAND_LOOPZ,
  OPERAND_MOV,
  OPERAND_SUB,
  // ---------------------
  OPERAND_COUNT,
};

static string_t operand_names[OPERAND_COUNT] = {
  STRING_LIT("UNKNOWN"),
  STRING_LIT("add"),
  STRING_LIT("cmp"),
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
  STRING_LIT("sub"),
};

enum register_t {
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
  // --------------
  REGISTER_COUNT,
};

static string_t register_names[REGISTER_COUNT] = {
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
};

// TODO: remove
string_t unknown_str = STRING_LIT("???");     

static string_t address_names[8] = {
  STRING_LIT("bx + si"),
  STRING_LIT("bx + di"),
  STRING_LIT("bp + si"),
  STRING_LIT("bp + di"),
  STRING_LIT("si"),
  STRING_LIT("di"),
  STRING_LIT("bp"),
  STRING_LIT("bx"),
};

static uint8_t next_byte(string_t* instruction_stream) {
  assert(instruction_stream->length > 0 && "Unexpected end of instruction stream");

  uint8_t byte = *(instruction_stream->data);
  instruction_stream->data   += 1;
  instruction_stream->length -= 1;

  return byte;
}

static int16_t next_data_s16(string_t *instruction_stream) {
  uint8_t lo = next_byte(instruction_stream);
  uint8_t hi = ((lo & 0x80) ? 0xff : 0);
  int16_t data = (((int16_t)hi) << 8) | (int16_t)lo;
  return data;
}

static int8_t next_data_s8(string_t *instruction_stream) {
  int8_t data = (int8_t)next_byte(instruction_stream);
  return data;
}

static uint16_t next_data(string_t* instruction_stream, uint8_t w) {
  uint8_t lo = next_byte(instruction_stream);
  uint8_t hi = (w == 1) ? next_byte(instruction_stream) : ((lo & 0x80) ? 0xff : 0);

  uint16_t data = (((int16_t)hi) << 8) | (uint16_t)lo;

  return data;
}

// TODO: probably just a uint here
static string_t next_address(string_t* instruction_stream, uint8_t w) {
  // TODO: addresses are always wide?
  uint16_t data = next_data(instruction_stream, w);
  string_t address = string_pushf("[%d]", data);
  return address;
}

static string_t next_effective_address(string_t *instruction_stream, uint8_t mod, uint8_t rm) {
  string_list_t address_builder = {0};//PUSH_ARRAY(string_list_t, 1);
  string_list_push(&address_builder, string_cstring("["));
  if (mod == 0 && rm == 6) {
    uint16_t data = next_data(instruction_stream, 1);
    assert(data != 0 && "Require a direct address in an effective address calculation");

    string_list_pushf(&address_builder, "%d", data);
  } else {
    string_list_push(&address_builder, address_names[rm]);
    if (mod) {
      uint16_t data = next_data(instruction_stream, mod == 2 ? 1 : 0);

      // only print if offset isn't 0
      if (data) {
        string_list_pushf(
          &address_builder, " %s %d",
          data < 0 ? "-" : "+",
          data < 0 ? (data * -1) : data
        );
      }
    }
  }
  string_list_push(&address_builder, string_cstring("]"));
  string_t address = string_list_join(&address_builder);
  return address;
}

static string_t next_register_memory(string_t *instruction_stream, uint8_t w, uint8_t mod, uint8_t rm) {
  string_t result = (mod == 3) ? register_names[(w * 8) + rm] : next_effective_address(instruction_stream, mod, rm);
  return result;
}

int main(int argc, char** argv) {
  if (argc == 1) {
    fprintf(stderr, "Usage: sim[filename]\n");
    return 1;
  }

  global_memory_base = (uint8_t *)malloc(global_memory_size);
  assert(global_memory_base && "Failed to allocate data");
  memset(global_memory_base, 0, global_memory_size);
  global_memory = global_memory_base;

  char* filename = argv[1];
  string_t instruction_stream = read_entire_file(filename);
  assert(instruction_stream.length != 0 && "Failed to read file");

  int print_bytes = argc > 2;

  printf("; %s\n", filename);
  printf("\n");
  if (!print_bytes) {
    printf("bits 16");
    printf("\n");
  }

  uint8_t *start = instruction_stream.data;

  struct cmd_t {
    cmd_t *first;

    cmd_t *next;
    cmd_t *prev;

    size_t ip;
    string_t text;
  };

  struct label_t {
    size_t ip;
    string_t label;
  };

  label_t labels[4];
  size_t label_count = 0;

  cmd_t *last_cmd = NULL;
  for (;;) {
    if (instruction_stream.length == 0) break;

    size_t ip = (size_t)(instruction_stream.data - start);

    operand_t operand;
    string_t dest = { 0 };
    string_t source = { 0 };

    uint8_t* instruction_stream_start = instruction_stream.data;

    uint8_t b1 = next_byte(&instruction_stream);
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
        uint8_t d = (b1 >> 1) & 1;
        uint8_t w = (b1 >> 0) & 1;

        uint8_t b2 = next_byte(&instruction_stream);
        uint8_t mod = (b2 >> 6);
        uint8_t reg = (b2 >> 3) & 7;
        uint8_t rm  = (b2 >> 0) & 7;

        string_t register_memory_name = next_register_memory(&instruction_stream, w, mod, rm);

        switch (b1) {
          case 0x00:
          case 0x01:
          case 0x02:
          case 0x03: operand = OPERAND_ADD; break;

          case 0x28:
          case 0x29:
          case 0x2A:
          case 0x2B: operand = OPERAND_SUB; break;

          case 0x38:
          case 0x39:
          case 0x3A:
          case 0x3B: operand = OPERAND_CMP; break;

          case 0x88:
          case 0x89:
          case 0x8A:
          case 0x8B: operand = OPERAND_MOV; break;

          default: 
            printf("\n!!! Unexpected operand 0x%x\n", b1);
            assert(false && "Unexpected operand");
            break;
        }

        dest    = d ? register_names[w * 8 + reg] : register_memory_name;
        source  = d ? register_memory_name : register_names[w * 8 + reg];
      } break;

      case 0x04: // ADD b,ia
      case 0x05: // ADD w,ia
      case 0x2C: // SUB b,ia
      case 0x2D: // SUB w,ia
      case 0x3C: // CMP b,ia
      case 0x3D: // CMP w,ia
      {
        uint8_t w = (b1 >> 0) & 1;

        uint16_t data = next_data(&instruction_stream, w);

        switch (b1) {
          case 0x04:
          case 0x05: operand = OPERAND_ADD; break;

          case 0x2C:
          case 0x2D: operand = OPERAND_SUB; break;

          case 0x3C:
          case 0x3D: operand = OPERAND_CMP; break;

          default: 
            printf("\n!!! Unexpected operand 0x%02x\n", b1);
            assert(false && "Unexpected operand");
            break;
        }
        switch (b1) {
          case 0x04:
          case 0x2C:
          case 0x3C: dest = register_names[REGISTER_AL]; break;

          case 0x05:
          case 0x2D:
          case 0x3D: dest = register_names[REGISTER_AX]; break;

          default: 
            printf("\n!!! Unexpected dest 0x%02x\n", b1);
            assert(false && "Unexpected dest");
            break;
        }
        source = string_pushf("%d", data);
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
        int8_t inc = next_data_s8(&instruction_stream);

        size_t label_ip = (instruction_stream.data - start) + inc;
        assert(label_count < 4 && "Too many labels");

        string_t label = { 0 };
        for (size_t i = 0; i < label_count; i += 1) {
          if (labels[i].ip == label_ip) {
            label = labels[i].label;
            break;
          }
        }

        if (!label.data) {
          label = string_pushf("label%d", label_ip);
          labels[label_count++] = { label_ip, label };
        }

        switch(b1) {
          case 0x70: operand = OPERAND_JO;     break;
          case 0x71: operand = OPERAND_JNO;    break;
          case 0x72: operand = OPERAND_JB;     break;
          case 0x73: operand = OPERAND_JNB;    break;
          case 0x74: operand = OPERAND_JE;     break;
          case 0x75: operand = OPERAND_JNZ;    break;
          case 0x76: operand = OPERAND_JBE;    break;
          case 0x77: operand = OPERAND_JA;     break;
          case 0x78: operand = OPERAND_JS;     break;
          case 0x79: operand = OPERAND_JNS;    break;
          case 0x7A: operand = OPERAND_JP;     break;
          case 0x7B: operand = OPERAND_JNP;    break;
          case 0x7C: operand = OPERAND_JL;     break;
          case 0x7D: operand = OPERAND_JNL;    break;
          case 0x7E: operand = OPERAND_JLE;    break;
          case 0x7F: operand = OPERAND_JG;     break;
          case 0xE0: operand = OPERAND_LOOPNZ; break;
          case 0xE1: operand = OPERAND_LOOPZ;  break;
          case 0xE2: operand = OPERAND_LOOP;   break;
          case 0xE3: operand = OPERAND_JCXZ;   break;

          default:
            printf("\n!!! Unexpected operand 0x%02x\n", b1);
            assert(false && "Unexpected operand");
            break;
        }
        dest = string_pushf("%.*s ; %d", STRING_FMT(label), inc);
      } break;

      case 0x80: // Immed b,r/m
      case 0x81: // Immed w,r/m
      case 0x82: // Immed b,r/m
      case 0x83: // Immed is,r/m
      {
        uint8_t w = (b1 >> 0) & 1;

        uint8_t b2 = next_byte(&instruction_stream);
        uint8_t mod = (b2 >> 6);
        uint8_t op  = (b2 >> 3) & 7;
        uint8_t rm  = (b2 >> 0) & 7;

        string_t register_memory_name = next_register_memory(&instruction_stream, w, mod, rm);

        uint16_t data;
        if (b1 == 0x83) {
          data = next_data_s16(&instruction_stream);
        } else {
          data = next_data(&instruction_stream, w);
        }

        switch (op) {
          case 0: operand = OPERAND_ADD; break;
          case 1: assert(false && "(not used)");
          case 2: assert(false && "TODO: adc");
          case 3: assert(false && "TODO: sbb");
          case 4: assert(false && "(not used)");
          case 5: operand = OPERAND_SUB; break;
          case 6: assert(false && "(not used)");
          case 7: operand = OPERAND_CMP; break;
          default: assert(false);
        }
        dest = register_memory_name;
        // TODO: don't always do the size (only for memory, not register)
        source = string_pushf("%s %d", w ? "word" : "byte", data);
      } break;

      case 0xA0: // MOV m -> AL
      case 0xA1: // MOV m -> AX
      case 0xA2: // MOV AX -> m
      case 0xA3: // MOV AX -> m
      {
        uint8_t d = (b1 >> 1) & 1;
        uint8_t w = (b1 >> 0) & 1;

        string_t address = next_address(&instruction_stream, w);
        register_t reg = w ? REGISTER_AX : REGISTER_AL;

        operand = OPERAND_MOV;
        dest    = d ? address : register_names[reg];
        source  = d ? register_names[reg] : address;
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
        uint8_t w = b1 >= 0xB8; // TODO: does this "scale" (i.e. work for all values? probably not)

        uint16_t data = next_data(&instruction_stream, w);

        operand = OPERAND_MOV;
        dest    = register_names[b1 - 0xB0];
        source  = string_pushf("%d", data);
      } break;

      case 0xC6: // MOV b,i,r/m
      case 0xC7: // MOV w,i,r/m
      {
        uint8_t w = (b1 >> 0) & 1;

        uint8_t b2 = next_byte(&instruction_stream);
        uint8_t mod = (b2 >> 6);
        // op?
        uint8_t rm  = (b2 >> 0) & 7;

        string_t address = next_effective_address(&instruction_stream, mod, rm);
        uint16_t data    = next_data(&instruction_stream, w);

        operand = OPERAND_MOV;
        dest    = address;
        source  = string_pushf("%s %d", w ? "word" : "byte", data);
      } break;


      default:
        printf("\n!!! Unexpected instruction  %s  0x%x\n", bit_string_u8(b1), b1);
        assert(false && "Unexpected instruction");
        break;
    }

    if (print_bytes) {
      size_t len = instruction_stream.data - instruction_stream_start;
      // printf(" %s ", bit_string_u8(*instruction_stream_start));
      printf("%3zd  ", ip);
      for (size_t i = 0; i < 6; i += 1) {
        if (i < len) {
          printf("%02x ", *(instruction_stream_start + i));
        } else {
          printf("   ");
        }
      }
      printf("\n");
    }

    string_t text;
    if (source.length) {
      text = string_pushf("%.*s %.*s, %.*s", STRING_FMT(operand_names[operand]), STRING_FMT(dest), STRING_FMT(source));
    } else {
      text = string_pushf("%.*s %.*s", STRING_FMT(operand_names[operand]), STRING_FMT(dest));
    }

    cmd_t *cmd = PUSH_ARRAY(cmd_t, 1);
    cmd->ip = ip;
    cmd->text = text;

    if (last_cmd) {
      cmd->prev = last_cmd;
      cmd->first = last_cmd->first;
      last_cmd->next = cmd;
    } else {
      cmd->first = cmd;
    }
    last_cmd = cmd;
  }

  cmd_t *cmd = last_cmd->first;
  while (cmd != NULL) {
    for (size_t label_index = 0; label_index < label_count; label_index += 1) {
      label_t label = labels[label_index];
      if (label.ip == cmd->ip) {
        printf("%.*s:\n", STRING_FMT(label.label));
        break;
      }
    }

    printf("%.*s\n", STRING_FMT(cmd->text));
    cmd = cmd->next;
  }

  printf("; Used %zd bytes\n", (size_t)(global_memory - global_memory_base));

  return 0;
}
