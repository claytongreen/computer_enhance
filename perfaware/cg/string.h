#ifndef _STRING_H
#define _STRING_H

#include "types.h"


struct string_t {
  u8* data;
  size_t length;
};

struct string_list_node_t {
  string_list_node_t* next;
  string_t string;
};

struct string_list_t {
  string_list_node_t* first;
  string_list_node_t* last;
  size_t node_count;
  size_t total_length;
};


string_t string_create(u8* data, size_t length);
string_t string_cstring(const char *s);

void string_list_push(string_list_t *list, string_t string);
string_list_t string_split(string_t string, u8 split);
string_t string_list_join(string_list_t *list);

string_t string_pushfv(const char *fmt, va_list args);
string_t string_pushf(const char *fmt, ...);

void string_list_pushf(string_list_t *list, const char* fmt, ...);

// -----------------------------------------------------------------------------

static string_t _string_create(uint8_t *data, size_t length) {
  string_t result = { data, length };
  return result;
}

#define STRING_LIT(s) _string_create((u8 *)(s), sizeof(s) - 1)
#define STRING_FMT(s) (int)((s).length), (s).data

// TODO: where should these go??
static string_t str_empty = STRING_LIT("");
static string_t str_none = STRING_LIT("none");


char* bit_string_u8(u8 byte);


#endif // _STRING_H
