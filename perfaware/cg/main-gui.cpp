/// 

#include "raylib.h"

#include "types.h"

#include "mem.h"
#include "string.h"
#include "instruction.h"
#include "sim.h"
#include "os.h"

#include "mem.cpp" // TODO: proper area allocator
#include "string.cpp"
#include "instruction.cpp"
#include "sim.cpp"
#include "printer.cpp"
#include "os.cpp"

static const int screen_width = 1920;
static const int screen_height = 1080;

#include "draw.cpp"

static void ui_load_file(simulator_t *sim, ui_t *ui, char *file) {
  sim_reset(sim);

  arena_reset(ui->arena);

  string_list_t parts = string_split(ui->frame_arena, string_cstring(ui->frame_arena, file), '\\');
  string_t filename = parts.last->string;

  string_t obj = read_entire_file(ui->frame_arena, file);
  if (obj.length) {
    sim_load(sim, obj);

    ui->filename.length = filename.length;
    ui->filename.data = PUSH_ARRAY(ui->arena, u8, ui->filename.length);
    memcpy(ui->filename.data, filename.data, ui->filename.length);

    u32 max_instruction_count = 1000;
    ui->instructions = PUSH_ARRAY(ui->arena, instruction_t, max_instruction_count);
    ui->instruction_count = 0;
    for (;;) {
      if ((sim->memory + sim->ip) > sim->code_end) break;

      instruction_t instruction = instruction_decode(sim, sim->ip);

      ui->instructions[ui->instruction_count++] = instruction;

      sim->ip += instruction.bytes_count;

      if (instruction.bytes_count > ui->max_instruction_width) {
        ui->max_instruction_width = instruction.bytes_count;
      }
    }

    printf("Loaded %d instructions\n", ui->instruction_count);
  } else {
    SIM_ERROR(sim, "Failed to open file: %.*s", STRING_FMT(filename));
  }
}

int main(void) {
  init_register_map();

  ui_t ui = {};
  ui.arena = arena_create();
  ui.frame_arena = arena_create();

  simulator_t sim = {};
  sim.arena = arena_create();
  sim.memory = PUSH_ARRAY(sim.arena, u8, MB(1));

  ui_load_file(&sim, &ui, "..\\part1\\listing_0047_challenge_flags");

  InitWindow(screen_width, screen_height, "EightyEightSix");
  SetTargetFPS(144);

  b32 should_draw = 1;
  b32 running = 0;
  while (!WindowShouldClose()) {
    arena_reset(ui.frame_arena);

    // input + update
    if (running || IsKeyPressed(KEY_F10)) {
      if ((sim.memory + sim.ip) >= sim.code_end) {
        if (!running) {
          sim_reset(&sim);
        }
        running = 0;
      } else {
        sim_step(&sim);
      }

      should_draw = 1;
    } else if (IsKeyPressed(KEY_F5)) {
      sim_reset(&sim);
      running = 1;
    }

    if (IsFileDropped()) {
      FilePathList files = LoadDroppedFiles();
      if (files.count) {
        char *path = files.paths[0];
        ui_load_file(&sim, &ui, path);
      }
      UnloadDroppedFiles(files);
    }

    // RENDER
    if (should_draw) {
      // should_draw = 0;

      BeginDrawing();
      ClearBackground(DARKGRAY);
      draw(&sim, &ui);
      EndDrawing();
    }
  }

  CloseWindow();

  return 0;
}
