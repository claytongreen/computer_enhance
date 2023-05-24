/// 

#include "raylib.h"

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

static const int screen_width = 1920;
static const int screen_height = 1080;

#include "draw.cpp"

static void ui_decode_instructions(simulator_t *sim, ui_t *ui, u32 offset) {
  u32 instruction_count = 0;
  s8 max_instruction_width = 0;

  u32 ip = offset;
  for (;;) {
    // TODO: more explicit way to define end of program?
    if (ip >= sim->code_end) break;

    instruction_t instruction = instruction_decode(sim, ip);

    ui->instructions[instruction_count++] = instruction;

    ip += instruction.bytes_count;

    if (instruction.bytes_count > max_instruction_width) {
      max_instruction_width = instruction.bytes_count;
    }
  }

  ui->instruction_count = instruction_count;
  ui->max_instruction_width = max_instruction_width;
}

static void ui_load_file(simulator_t *sim, ui_t *ui, char *file) {
  sim_reset(sim);

  arena_reset(ui->arena);

  ui->instructions = PUSH_ARRAY(ui->arena, instruction_t, 1000);

  string_list_t parts = string_split(ui->frame_arena, string_cstring(ui->frame_arena, file), '/');
  string_t filename = parts.last->string;

  string_t obj = read_entire_file(ui->frame_arena, file);
  if (obj.length) {
    sim_load(sim, obj);

    ui->filename = filename;

    ui_decode_instructions(sim, ui, 0);

    printf("Loaded %d instructions\n", ui->instruction_count);
  } else {
    SIM_ERROR(sim, "Failed to open file: %.*s", STRING_FMT(filename));
  }
}

static void ui_step(simulator_t *sim, ui_t *ui, b32 verbose) {
  arena_temp_t temp = arena_temp_begin(ui->frame_arena);
  arena_t *arena = temp.arena;

  instruction_t instruction = instruction_decode(sim, sim->ip);
  if (!sim->error.length) {
    u32 ip_before = sim->ip;
    u16 flags_before = sim->flags;

    u16 registers_before[8];
    for (u32 i = 0; i < 8; i += 1) {
      registers_before[i] = sim->registers[i];
    }

    instruction_simulate(sim, instruction);

    if (verbose) {
      string_list_t sb = {};
      string_t a = print_instruction(arena, sim, instruction); 
      string_list_push(arena, &sb, a);
      string_list_push(arena, &sb, STRING_LIT(" ; "));

      register_t registers[] = { REGISTER_AX, REGISTER_BX, REGISTER_CX, REGISTER_DX, REGISTER_SP, REGISTER_BP, REGISTER_SI, REGISTER_DI };
      for (u32 i = 0; i < 8; i += 1) {
        u16 before = registers_before[i];
        u16 after = sim->registers[i];
        if (before != after) {
          register_t reg = registers[i];
          string_list_pushf(arena, &sb, "%.*s:0x%x->0x%x ", STRING_FMT(register_names[reg]), before, after);
        }
      }

      string_list_pushf(arena, &sb, "ip:0x%x->0x%x ", ip_before, sim->ip);

      if (flags_before != sim->flags) {
        string_t before = print_flags(arena, flags_before);
        string_t after = print_flags(arena, sim->flags);
        string_list_pushf(arena, &sb, "flags:%.*s->%.*s ", STRING_FMT(before), STRING_FMT(after));
      }

      string_t text = string_list_join(arena, &sb, STRING_LIT(""));
      printf("%.*s\n", STRING_FMT(text));
    }
  }

  arena_temp_end(temp);
}


int main(void) {
  init_register_map();

  ui_t ui            = {};
  ui.arena           = arena_create();
  ui.frame_arena     = arena_create();
  ui.load_file_index = -1;

  simulator_t sim = {};
  sim.arena       = arena_create();
  sim.memory      = PUSH_ARRAY(sim.arena, u8, MB(1));

  {
    // char *f = "../part1/listing_0055_challenge_rectangle";
    char *f = "../../part1/listing_0041_add_sub_cmp_jnz";
    ui_load_file(&sim, &ui, f);
  }

  InitWindow(screen_width, screen_height, "EightyEightSix");
  SetTargetFPS(144);

  b32 should_draw = 1;
  b32 running = 0;
  while (!WindowShouldClose()) {
    arena_reset(ui.frame_arena);

    // input + update
    if ((running || IsKeyPressed(KEY_F10)) && (sim.error.length == 0)) {
      if (sim.ip >= sim.code_end) {
        if (!running) {
          sim_reset(&sim);
        }
        running = 0;
      } else {
        u32 steps = running ? 10000 : 1;

        while (steps--) {
          ui_step(&sim, &ui, 0);

          if (sim.error.length || (sim.ip >= sim.code_end)) {
            running = 0;
            break;
          }

          ui_decode_instructions(&sim, &ui, 0);
        }
      }

      should_draw = 1;
    } else if (IsKeyPressed(KEY_F5)) {
      if (running) {
        running = 0;
      } else {
        sim_reset(&sim);
        running = 1;
      }
    }

    if (IsFileDropped()) {
      FilePathList files = LoadDroppedFiles();
      if (files.count) {
        char *path = files.paths[0];
        ui_load_file(&sim, &ui, path);
      }
      UnloadDroppedFiles(files);
    }

    if (ui.opening && !ui.files_count) {
      FilePathList files = LoadDirectoryFiles("../../part1");

      string_list_t names = {};

      for (u32 i = 0; i < files.count; i += 1) {
        char *path = files.paths[i];

        string_list_t path_parts         = string_split(ui.frame_arena, string_cstring(ui.frame_arena, path), '/');
        string_t      filename           = path_parts.last->string;
        string_list_t name_and_extension = string_split(ui.frame_arena, filename, '.');
        if (name_and_extension.node_count == 1) {
          string_t name = name_and_extension.first->string;
          string_list_push(ui.frame_arena, &names, name);
        }
      }

      u8 *mem = PUSH_ARRAY(ui.arena, u8, names.total_length + names.node_count);

      ui.files       = PUSH_ARRAY(ui.arena, u8 *, names.node_count);
      ui.files_count = names.node_count;

      u32 files_i = 0;
      for (string_list_node_t *node = names.first; node; node = node->next) {
        string_t s = node->string;

        memcpy(mem, s.data, s.length);

        ui.files[files_i++] = mem;

        mem += s.length + 1;
      }

      UnloadDirectoryFiles(files);
    }

    if (ui.load_file_index != -1) {
      char path[256];
      sprintf(path, "../../part1/%s", ui.files[ui.load_file_index]);
      ui_load_file(&sim, &ui, path);
      ui.load_file_index = -1;
    }

    // RENDER
    if (should_draw) {
      // should_draw = 0;

      BeginDrawing();

      draw(&sim, &ui);

      EndDrawing();
    }
  }

  CloseWindow();

  return 0;
}
