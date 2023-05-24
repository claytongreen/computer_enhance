#include "string.h"
#include "mem.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static string_t string_create(arena_t *arena, uint8_t* data, size_t length) {
  string_t* string = PUSH_ARRAY(arena, string_t, 1);
  string->data = data;
  string->length = length;
  return *string;
}

static string_t string_cstring(arena_t *arena, const char* s) {
  string_t string = string_create(arena, (uint8_t*)s, strlen(s));
  return string;
}

static void string_list_push(arena_t *arena, string_list_t* list, string_t string) {
  string_list_node_t* node = PUSH_ARRAY(arena, string_list_node_t, 1);
  node->string = string;

  if (list->last) {
    list->last->next = node;
    list->last = node;
  } else {
    list->first = node;
    list->last = node;
  }

  list->node_count += 1;
  list->total_length += string.length;
}

// TODO: split on string instead of char
static string_list_t string_split(arena_t *arena, string_t string, u8 split) {
  string_list_t result = {};

  u8 *at = string.data;
  u8 *end = string.data + string.length;

  u8 *start = at;
  while (at != end) {
    if (*at == split) {
      string_t part = { start, (size_t)(at - start) };
      string_list_push(arena, &result, part);
      at += 1; // skip the split
      start = at;
    }

    at += 1;
  }

  string_t part = { start, (size_t)(at - start) };
  string_list_push(arena, &result, part);

  return result;
}

static string_t string_list_join(arena_t *arena, string_list_t* list, string_t join) {
  // TODO: handle putting things in between list parts
  u64 length = list->total_length;
  if (join.length) {
    length += (list->node_count - 1) * join.length;
  }

  string_t result = {};
  result.data   = PUSH_ARRAY(arena, uint8_t, length);
  result.length = length;

  u8 *at = result.data;
  for (string_list_node_t *node = list->first; node != 0; node = node->next) {
    string_t part = node->string;
    memcpy(at, part.data, part.length);
    at += part.length;
    if (node->next) {
      memcpy(at, join.data, join.length);
      at += join.length;
    }
 }

  return result;
}

static string_t string_pushfv(arena_t *arena, const char* fmt, va_list args) {
  // in case we need to try a second time
  va_list args2;
  va_copy(args2, args);

  // try to build the string in 1024 bytes
  size_t buffer_size = 1024;
  uint8_t* buffer = PUSH_ARRAY(arena, uint8_t, buffer_size);
  size_t actual_size = (size_t)vsnprintf((char*)buffer, buffer_size, fmt, args);

  string_t result = {};
  if (actual_size < buffer_size) {
    arena_pop_to(arena, arena->pos - (buffer_size - actual_size - 1));
    result = string_create(arena, buffer, actual_size);
  } else {
    arena_pop_to(arena, arena->pos - buffer_size);
    uint8_t* actual_size_buffer = PUSH_ARRAY(arena, uint8_t, actual_size + 1);
    size_t final_size = vsnprintf((char*)actual_size_buffer, actual_size + 1, fmt, args2);
    result = string_create(arena, actual_size_buffer, final_size);
  }
  va_end(args2);

  return result;
}

static string_t string_pushf(arena_t *arena, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  string_t result = string_pushfv(arena, fmt, args);

  va_end(args);

  return result;
}

static void string_list_pushf(arena_t *arena, string_list_t* list, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  string_t string = string_pushfv(arena, fmt, args);

  va_end(args);

  string_list_push(arena, list, string);
}

// -----------------------------------------------------------------------------

static char bits[20] = "0000_0000 0000_0000";
char* bit_string_u8(u8 byte) {
  for (size_t i = 0; i < 4; i += 1) {
    bits[i] = (byte >> (7 - i)) & 1 ? '1' : '0';
  }
  for (size_t i = 4; i < 8; i += 1) {
    bits[1 + i] = (byte >> (7 - i)) & 1 ? '1' : '0';
  }

  bits[9] = 0;

  return bits;
}

char *bit_string16(u16 word) {
  for (size_t i = 0; i < 4; i += 1) {
    bits[i] = (word >> (15 - i)) & 1 ? '1' : '0';
  }
  for (size_t i = 4; i < 8; i += 1) {
    bits[1 + i] = (word >> (15 - i)) & 1 ? '1' : '0';
  }
  bits[9] = ' ';
  for (size_t i = 8; i < 12; i += 1) {
    bits[2 + i] = (word >> (15 - i)) & 1 ? '1' : '0';
  }
  for (size_t i = 12; i < 16; i += 1) {
    bits[3 + i] = (word >> (15 - i)) & 1 ? '1' : '0';
  }

  return bits;
}

