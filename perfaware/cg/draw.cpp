#include "raylib.h"
#define RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT 32

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"


#include "sim.h"

struct ui_t {
  string_t filename;

  instruction_t *instructions;
  u32 instruction_count;

  s8 max_instruction_width;

  b32 opening;
  u8 **files;
  u64 files_count;

  u64 load_file_index;

  arena_t *frame_arena;
  arena_t *arena;
};


static Font font;


static float line_height = TEXT_SIZE * 1.5;

static void Text(const char *text, float x, float y, Color c) {
  // DrawTextEx(font, text, { x, y }, text_size, 0, c);
  Rectangle bounds = { x, y, (float)GetTextWidth(text), TEXT_SIZE };
  GuiDrawText(text, bounds, 0, c);
}

static void draw_instructions(simulator_t *sim, ui_t *ui) {
  instruction_t *instructions = ui->instructions;
  u32 instruction_count = ui->instruction_count; 

  Rectangle rect = { 0, 0, screen_width / 2.0f, screen_height};

  GuiPanel(rect, TextFormat("%.*s", STRING_FMT(ui->filename)));

  int instructions_height = instruction_count * line_height;

  static Vector2 scroll;
  static Rectangle scroll_rec = { 0, (float)RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT, screen_width / 2.0f, screen_height - (float)RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT };
  static Rectangle scroll_content_rec = { 0, 0, scroll_rec.width - 2, instructions_height * 1.5f };
  Rectangle view = GuiScrollPanel(scroll_rec, NULL, scroll_content_rec, &scroll);
  BeginScissorMode(view.x, view.y, view.width, view.height);

  int text_x = scroll_rec.x + TEXT_PADDING;
  int text_y = scroll_rec.y + TEXT_PADDING + scroll.y;

  int address_width = GetTextWidth("000000");
  int byte_width = GetTextWidth("00");

  for (s32 i = 0; i < instruction_count; i += 1) {
    instruction_t instruction = instructions[i];
    b32 current = (sim->memory + sim->ip) == instruction.ip;

    if (current) {
      DrawRectangleRec({ rect.x, (float)text_y, rect.width, TEXT_SIZE }, Fade(RED, 0.1f));
    }

    int x = text_x;

    { // address
      const char *text = TextFormat("%06x", instruction.ip - sim->memory);
      Text(text, x, text_y, LIGHTGRAY);
      x += address_width + TEXT_PADDING;
    }
    { // instruction bytes
      for (u8 *b = instruction.ip; b < (instruction.ip + instruction.bytes_count); b += 1) {
        const char *text = TextFormat("%02x", *b);
        Text(text, x, text_y, DARKGRAY);
        x += byte_width * 1.25;
      }
      x = text_x + address_width + ((ui->max_instruction_width + 1) * byte_width * 1.25);
    }
    { // decoded instruction
      string_t s = print_instruction(ui->frame_arena, sim, instruction);
      const char *text = TextFormat("%.*s", STRING_FMT(s));
      Color c = current ? RED : BLACK;
      Text(text, x, text_y, c);
    }

    text_y += line_height;
  }

  EndScissorMode();
}

static float registers_height = (float)RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + (line_height * 13) + (TEXT_PADDING * 4);

static void draw_registers(simulator_t *sim) {
  int reg_width = GetTextWidth("xx");
  int val_width = GetTextWidth("0x0000");
  int ival_width = GetTextWidth("-32767");
  // int c = GetTextWidth("0000_0000 0000_0000", text_size);

  Rectangle rect = {
    screen_width  / 2.0f,
    0.0f,
    screen_width  / 2.0f,
    registers_height,
  };

  GuiPanel(rect, "Registers");

  rect.x += TEXT_PADDING;
  rect.y += (float)RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + TEXT_PADDING;

  {
    Text("IP", rect.x, rect.y, DARKGRAY);

    const char *text = TextFormat("0x%04x", sim->ip);
    Text(text, rect.x + reg_width + TEXT_PADDING, rect.y, BLACK);
  }

  rect.y += TEXT_SIZE + TEXT_PADDING;
  {
    register_t registers[] = { REGISTER_AX, REGISTER_BX, REGISTER_CX, REGISTER_DX };
    for (s32 reg_idx = 0; reg_idx < 4; reg_idx += 1) {
      int x = rect.x;

      register_t reg = (register_t)registers[reg_idx];
      b32 changed = 0; // TODO
      u16 value = register_get(sim, reg);

      // TODO: handle value change for H/L

      const char *text = TextFormat("%.*s", STRING_FMT(register_names[reg]));
      Text(text, x, rect.y, DARKGRAY);
      x += reg_width + TEXT_PADDING * 2.0f;

      Color c = changed ? RED : BLACK;
      text = TextFormat("0x%04x", value);
      Text(text, x, rect.y, c);
      x += val_width + TEXT_PADDING;

      text = TextFormat("%d", value);
      Text(text, x, rect.y, DARKGRAY);
      x += ival_width + TEXT_PADDING;

      Text(bit_string16(value), x, rect.y, DARKGRAY);

      rect.y += line_height;
    }
  }

  rect.y += TEXT_PADDING;
  {
    int x = rect.x;

    register_t registers[] = { REGISTER_ES, REGISTER_CS, REGISTER_SS, REGISTER_DS };
    s32 registers_count = ARRAY_COUNT(registers);
    for (s32 reg_idx = 0; reg_idx < 4; reg_idx += 1) {
      int x = rect.x;

      register_t reg = (register_t)registers[reg_idx];
      b32 changed = 0; // TODO
      u16 value = register_get(sim, reg);

      // TODO: handle value change for H/L

      const char *text = TextFormat("%.*s", STRING_FMT(register_names[reg]));
      Text(text, x, rect.y, DARKGRAY);
      x += reg_width + TEXT_PADDING * 2.0f;

      Color c = changed ? RED : BLACK;
      text = TextFormat("0x%04x", value);
      Text(text, x, rect.y, c);
      x += val_width + TEXT_PADDING;

      text = TextFormat("%d", value);
      Text(text, x, rect.y, DARKGRAY);
      x += ival_width + TEXT_PADDING;

      Text(bit_string16(value), x, rect.y, DARKGRAY);

      rect.y += line_height;
    }
  }

  rect.y += TEXT_PADDING;
  {
    int x = rect.x;

    register_t registers[] = { REGISTER_SP, REGISTER_BP, REGISTER_SI, REGISTER_DI };
    s32 registers_count = ARRAY_COUNT(registers);
    for (s32 reg_idx = 0; reg_idx < 4; reg_idx += 1) {
      int x = rect.x;

      register_t reg = (register_t)registers[reg_idx];
      b32 changed = 0; // TODO
      u16 value = register_get(sim, reg);

      // TODO: handle value change for H/L

      const char *text = TextFormat("%.*s", STRING_FMT(register_names[reg]));
      Text(text, x, rect.y, DARKGRAY);
      x += reg_width + TEXT_PADDING * 2.0f;

      Color c = changed ? RED : BLACK;
      text = TextFormat("0x%04x", value);
      Text(text, x, rect.y, c);
      x += val_width + TEXT_PADDING;

      text = TextFormat("%d", value);
      Text(text, x, rect.y, DARKGRAY);
      x += ival_width + TEXT_PADDING;

      Text(bit_string16(value), x, rect.y, DARKGRAY);

      rect.y += line_height;
    }
  }
}

static void draw_flags(simulator_t *sim) {
  float flag_size = 40;

  float x = screen_width / 2.0f;
  float y = registers_height;
  float width = screen_width / 2.0f;
  float height = (float)RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + TEXT_PADDING * 2.0f + flag_size;
  Rectangle rect = { x, y, width, height };

  GuiPanel(rect, "Flags");

  flag_t flags[] = { FLAG_CARRY, FLAG_PARITY, FLAG_AUXILIARY_CARRY, FLAG_ZERO, FLAG_SIGN, FLAG_OVERFLOW, };
  u32 flag_count = ARRAY_COUNT(flags);

  Rectangle r = { x + TEXT_PADDING, y + (float)RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + TEXT_PADDING, flag_size, flag_size };
  for (s32 i = 0; i < flag_count; i += 1) {
    flag_t flag = flags[i];

    b32 active = sim->flags & (1 << flag);
    Color bg = active ? LIME  : LIGHTGRAY;
    Color fg = active ? WHITE :  DARKGRAY;

    GuiDrawRectangle(r, 1, fg, bg);

    const char *text = TextFormat("%.*s", STRING_FMT(flag_names[flag]));
    GuiDrawText(text, r, TEXT_ALIGN_CENTER, fg);

    r.x += flag_size + TEXT_PADDING;
  }
}

static void draw_memory(simulator_t *sim) {
  static RenderTexture2D texture = LoadRenderTexture(64, 64);

  if (IsRenderTextureReady(texture)) {
    u8 *p = sim->memory + 256;
    UpdateTexture(texture.texture, p);
    float x = (screen_width / 2.0f) + 1.0f;
    float y = (screen_height / 2.0f) + 1.0f;
    float w = screen_width - x;
    float h = screen_height - y;
    if (w > h) {
      w = h;
    } else {
      h = w;
    }
    // DrawTexture(texture.texture, x, y, WHITE);
    DrawTexturePro(texture.texture, { 0, 0, 64, 64 }, { x, y, w, h }, { 0, 0 }, 0, WHITE);
  }
}

static void draw_load_file(ui_t *ui) {
  char *text = "Load...";
  float width = (float)GetTextWidth(text);
  Rectangle button_rect = {
    screen_width - width - TEXT_PADDING * 3,
    screen_height - TEXT_SIZE - TEXT_PADDING * 3,
    width + TEXT_PADDING * 2,
    TEXT_SIZE + TEXT_PADDING * 2,
  };
  if (GuiButton(button_rect, text)) {
    ui->opening = 1;
  }

  if (ui->opening) {
    float window_width = 600;
    float window_height = 500;
    Rectangle window_rect = {
      button_rect.x + button_rect.width - window_width,
      button_rect.y + button_rect.height - window_height,
      window_width,
      window_height,
    };
    text = (char *)TextFormat("Load... %d", ui->files_count);
    if (GuiWindowBox(window_rect, text)) {
      ui->opening = 0;
    }

    Rectangle list_rect = {
      window_rect.x,
      window_rect.y + RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT,
      window_rect.width,
      window_rect.height - RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT,
    };

    if (ui->files_count) {
      static int scroll_index;
      static int active = -1;
      static int focus;

      int alignment = GuiGetStyle(LISTVIEW, TEXT_ALIGNMENT);
      GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);

      active = GuiListViewEx(list_rect, (const char **)ui->files, ui->files_count, &focus, &scroll_index, active);
      if (active != -1) {
        ui->opening = 0;
        ui->load_file_index = active;
        active = -1;
      }

      GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, alignment);
    }
  }
}

static void draw_error(simulator_t *sim) {
  if (!sim->error.length) return;

  const char *text = TextFormat("%.*s", STRING_FMT(sim->error));
  int error_width = GetTextWidth(text);

  int e_x = (screen_width / 2) - (error_width / 2);
  int e_y = (screen_height / 2) - TEXT_PADDING;
  DrawRectangle(e_x - TEXT_PADDING, e_y - TEXT_PADDING, error_width + TEXT_PADDING, TEXT_PADDING + TEXT_SIZE + TEXT_PADDING, RED);
  Text(text, e_x, e_y, WHITE);
}

static void draw(simulator_t *sim, ui_t *ui) {
  static b32 init = 1;
  if (init) {
    init = 0;

    font = LoadFont("build\\FiraCode-Regular.ttf");
  }

  GuiSetFont(font);

  Rectangle window = {};
  window.width = screen_width;
  window.height = screen_height;

  draw_instructions(sim, ui);
  draw_registers(sim);
  draw_flags(sim);

  draw_memory(sim);

  draw_load_file(ui);

  draw_error(sim);
}
