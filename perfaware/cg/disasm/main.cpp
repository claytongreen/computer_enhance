#include "types.h"

#include "mem.h"
#include "string.h"
#include "instruction.h"
#include "sim.h"
#include "os.h"

#include "mem.cpp"
#include "string.cpp"
#include "instruction.cpp"
#include "printer.cpp"
#include "sim.cpp"
#include "os.cpp"

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
  b32 count = 0;

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
      } else if (strcmp(arg, "count") == 0) {
        count = 1;
      }
    } else {
      filepath = arg;
    }
  }

  if (!filepath) {
    print_usage(argv[0]);
    return 1;
  }
  if (!decode && !simulate && !count) {
    print_usage(argv[0]);
    return 1;
  }

  init_register_map();

  simulator_t sim = {};
  sim.arena = arena_create();
  sim.memory = PUSH_ARRAY(sim.arena, u8, MB(1));

  {
    arena_temp_t temp = arena_temp_begin(sim.arena);

    string_t binary = read_entire_file(temp.arena, filepath);
    if (binary.length == 0) {
      fprintf(stderr, "Failed to open %s\n", filepath);
      return 1;
    }

    sim_load(&sim, binary);

    arena_temp_end(temp);
  }

  u32 total_clocks = 0;
  u32 ip = 0;
  for (;;) {
    if ((sim.memory + ip) >= sim.code_end) break;

    instruction_t instruction = instruction_decode(&sim, ip);
    if (instruction.opcode == OP_CODE_INT) break;

    u32 clocks = instruction_estimate_clocks(&sim, instruction);

    arena_temp_t temp = arena_temp_begin(sim.arena);
    arena_t *arena = temp.arena;

    total_clocks += clocks;

    string_list_t sb = {};
    string_list_push(arena, &sb, print_instruction(arena, &sim, instruction));
    string_list_push(arena, &sb, STRING_LIT(" ; "));

    string_list_pushf(arena, &sb, " Clocks: +%d = %d ", clocks, total_clocks);

    string_list_push(arena, &sb, STRING_LIT("| "));

    // TODO: print registers
    // TODO: print ip

    ip += instruction.bytes_count;

    string_t s = string_list_join(arena, &sb, STRING_LIT(""));
    printf("%.*s\n", STRING_FMT(s));

    arena_temp_end(temp);

    if (sim.error.length) {
      printf(";;; %.*s\n", STRING_FMT(sim.error));
      break;
    }
  }

  return sim.error.length;
}

