#define _CRT_SECURE_NO_WARNINGS

#include <intrin.h>

#include "types.h"

#include "mem.h"
#include "string.h"
#include "instruction.h"
#include "sim.h"
#include "os.h"

#include "mem.cpp" // TODO: proper area allocator
#include "string.cpp"
#include "instruction.cpp"
#include "printer.cpp"
#include "sim.cpp"
#include "os.cpp"

static string_t read_entire_file(char *filename);

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

  b32 flags = 0;

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
      } else if (strcmp(arg, "flags") == 0) {
        flags = 1;
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

  string_t binary = read_entire_file(filepath);
  if (binary.length == 0) {
    fprintf(stderr, "Failed to open %s\n", filepath);
    return 1;
  }

  simulator_t sim = {0};
  sim.memory = PUSH_ARRAY(u8, 1024 * 1024);
  sim.instruction_stream = binary;

  init_register_map();

  // TODO: remove, just used to print because of jumps and such
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
    if (sim.ip >= sim.instruction_stream.length) break;

    instruction_t instruction = instruction_decode(&sim, sim.ip);
    sim.ip += instruction.bytes_count;

    string_t text;
    if (sim.error.length) {
      text = string_pushf("??? ; %.*s", STRING_FMT(sim.error));
    } else if (simulate) {
      // record state before simulate
      u16 flags_before = sim.flags;
      register_t reg = (instruction.dest.kind == OPERAND_KIND_REGISTER) ? instruction.dest.reg : REGISTER_NONE;
      u16 reg_before = register_get(&sim, reg);

      instruction_simulate(&sim, instruction);

      // TODO: move this to printer?
      if (sim.error.length) {
        text = string_pushf(" ; %.*s", STRING_FMT(sim.error));
      } else {
        string_list_t sb = {};
        string_list_push(&sb, instruction_print(&sim, instruction));
        string_list_push(&sb, STRING_LIT(" ; "));
        //
        // TODO: print ip

        if (reg != REGISTER_NONE) {
          u16 reg_after = register_get(&sim, reg);
          string_list_pushf(&sb, "%.*s:0x%x->0x%x ", STRING_FMT(register_names[reg]), reg_before, reg_after);
        }

        if (flags && flags_before != sim.flags) {
          string_t before = print_flags(flags_before);
          string_t after = print_flags(sim.flags);
          string_list_pushf(&sb, "flags:%.*s->%.*s ", STRING_FMT(before), STRING_FMT(after));
        }

        text = string_list_join(&sb);
      }
    } else {
      text = instruction_print(&sim, instruction);
    }

    // @TODO: this is only here because of labeled jmp's. But if we don't do
    // those, we can just print directly.
    cmd_t *cmd = PUSH_ARRAY(cmd_t, 1);
    cmd->start = instruction.ip;
    cmd->end = instruction.ip + instruction.bytes_count;
    cmd->text = text;

    if (last_cmd) {
      cmd->prev = last_cmd;
      cmd->first = last_cmd->first;
      last_cmd->next = cmd;
    } else {
      cmd->first = cmd;
    }
    last_cmd = cmd;

    if (sim.error.length) break;
  }

  // Print the stuff
string_list_t parts = string_split(string_cstring(filepath), '\\');
  string_t filename = parts.last->string;

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
      size_t ip = cmd->start - sim.instruction_stream.data;
      for (size_t label_index = 0; label_index < sim.label_count; label_index += 1) {
        label_t label = sim.labels[label_index];
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
    if (flags) {
      string_t flags = print_flags(sim.flags);
      printf("   flags: %.*s\n", STRING_FMT(flags));
    }
    printf("\n");
  }

  return sim.error.length;
}

