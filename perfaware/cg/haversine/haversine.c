#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/stat.h>

#include <intrin.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define TRACE 0
#define USE_MALLOC 0

static uint64_t os_timer_frequency(void) {
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  return frequency.QuadPart;
}

static uint64_t os_timer_read(void) {
  LARGE_INTEGER value;
  QueryPerformanceCounter(&value);
  return value.QuadPart;
}

static uint64_t cpu_timer_read(void) {
  return __rdtsc();
}

// TODO: no crt

////////////////////////////////////////////////////////////////////////////////

static double Square(double A) {
  double Result = (A * A);
  return Result;
}

static double RadiansFromDegrees(double Degrees) {
  double Result = 0.01745329251994329577 * Degrees;
  return Result;
}

#define EARTH_RADIUS 6372.8
// NOTE(casey): EarthRadius is generally expected to be 6372.8
static double ReferenceHaversine(double X0, double Y0, double X1, double Y1, double EarthRadius) {
  /* NOTE(casey): This is not meant to be a "good" way to calculate the
     Haversine distance. Instead, it attempts to follow, as closely as possible,
     the formula used in the real-world question on which these homework
     exercises are loosely based.
  */

  double lat1 = Y0;
  double lat2 = Y1;
  double lon1 = X0;
  double lon2 = X1;

  double dLat = RadiansFromDegrees(lat2 - lat1);
  double dLon = RadiansFromDegrees(lon2 - lon1);
  lat1 = RadiansFromDegrees(lat1);
  lat2 = RadiansFromDegrees(lat2);

  double a = Square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * Square(sin(dLon / 2));
  double c = 2.0 * asin(sqrt(a));

  double Result = EarthRadius * c;

  return Result;
}

////////////////////////////////////////////////////////////////////////////////

static double random_in(double min, double max) {
  double scale = rand() / (double)RAND_MAX;
  double result = min + scale * (max - min);
  return result;
}

static int generate_data(int32_t seed, int32_t num_coord_pairs) {
  srand(seed);

  // TODO: bufferred write to files

  char *json_filename = (char *)malloc(2048);
  snprintf(json_filename, 1024, "build/data_%d.json", num_coord_pairs);
  FILE *json = fopen(json_filename, "w+");
  if (!json) {
    fprintf(stderr, "Failed to open file: '%s'\n", json_filename);
    return 1;
  }

  char *haveranswer_filename = json_filename + 1024;
  snprintf(haveranswer_filename, 1024, "build/data_%d_haveranswer.f64", num_coord_pairs);
  FILE *haveranswer = fopen(haveranswer_filename, "wb+");
  if (!haveranswer) {
    fclose(json);

    fprintf(stderr, "Failed to open file: '%s'\n", haveranswer_filename);
    return 1;
  }

  fprintf(json, "{\n  \"coords\": [\n");

  double avg = 0;
  for (size_t i = 0; i < num_coord_pairs; i += 1) {
    double x0 = random_in(-180.0, 180.0);
    double y0 = random_in(-90.0, 90.0);
    double x1 = random_in(-180.0, 180.0);
    double y1 = random_in(-90.0, 90.0);

    fprintf(
        json,
        "    { \"x0\": %.15f, \"y0\": %.15f, \"x1\": %.15f, \"y1\": %.15f }",
        x0, y0, x1, y1);
    if (i < num_coord_pairs - 1) {
      fprintf(json, ",\n");
    }

    double haversine = ReferenceHaversine(x0, y0, x1, y1, EARTH_RADIUS);
    fwrite(&haversine, sizeof(double), 1, haveranswer);

    avg += haversine;
  }

  avg /= num_coord_pairs;
  fwrite(&avg, sizeof(double), 1, haveranswer);

  fclose(haveranswer);

  fprintf(json, "\n  ]\n}");
  fclose(json);

  printf("Random Seed:      %d\n", seed);
  printf("Coordinate Pairs: %d\n", num_coord_pairs);
  printf("Expected Average: %.15f\n", avg);
  printf("\n");
  printf("  Saved: '%s'\n", json_filename);
  printf("  Saved: '%s'\n", haveranswer_filename);
  printf("\n");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

typedef struct arena_t arena_t;
struct arena_t {
  uint8_t *memory;
  int64_t at;
  int64_t capacity;
};

static arena_t null_arena = { 0, 0, 0 };

static arena_t *arena_create(int64_t capacity) {
  arena_t *result = &null_arena;

  void *memory = malloc(capacity);
  if (memory) {
    result = (arena_t *)memory;
    result->memory  = memory;
    result->at = sizeof(arena_t);
    result->capacity = capacity;
  }

  return result;
}

static void *arena_push(arena_t *arena, int64_t size) {
  assert(arena->at + size < arena->capacity);

  void *result = arena->memory + arena->at;
  arena->at += size;

  return result;
}

#if USE_MALLOC

static void *push(int64_t size) {
  void *result = malloc(size);
  memset(result, 0, size);
  return result;
}

#define NEW(PARENT, TYPE) (TYPE *)push(sizeof(TYPE))
#define NEW_SIZE(PARENT, TYPE, SIZE) (TYPE *)push((SIZE))
#else
#define NEW(PARENT, TYPE) (TYPE *)arena_push((PARENT)->arena, sizeof(TYPE))
#define NEW_SIZE(PARENT, TYPE, SIZE) (TYPE *)arena_push((PARENT)->arena, (SIZE))
#endif

typedef struct string_t string_t;
struct string_t {
  char *data;
  int32_t length;
};

#define STR(s) (string_t){ (s), (int32_t)strlen((s)) }

static bool string_equals(string_t a, string_t b) {
  bool result = false;

  if (a.length == b.length) {
    int32_t i = 0;
    for (; i < a.length; i += 1) {
      if (a.data[i] != b.data[i]) {
        break;
      }
    }

    result = (i == a.length);
  }

  return result;
}

static string_t read_entire_file(char *filename) {
  string_t result = { 0 };

  FILE *file = fopen(filename, "rb");
  if (!file) {
    fprintf(stderr, "Failed to open file: '%s'\n", filename);
  } else {
    fseek(file, 0, SEEK_END);
    long length = ftell(file);

    result.data = (char *)malloc(length);
    if (!result.data) {
      fclose(file);
      fprintf(stderr, "Failed to allocate data for file: %ld\n", length);
    } else {
      fseek(file, 0, SEEK_SET);
      size_t bytes_read = fread(result.data, 1, length, file);
      if (bytes_read != length) {
        fprintf(stderr, "Read %lld bytes, expecteed %ld bytes\n", bytes_read, length);
      }
      // TODO: WHY?? bytes_read != length
      // if (bytes_read != length) {
      //  fclose(file);
      //  fprintf(stderr, "Failed to read entire file:  %lld != %ld\n", bytes_read, length);

      //  free(result.data);
      //  result.data = 0;
      //} else {
        result.length = length;
      //}

      fclose(file);
    }
  }

  return result;
}

typedef enum json_type_kind_t json_type_kind_t;
enum json_type_kind_t {
  JSON_TYPE_KIND_NONE,
  JSON_TYPE_KIND_NULL,
  JSON_TYPE_KIND_NUMBER,
  JSON_TYPE_KIND_STRING,
  JSON_TYPE_KIND_OBJECT,
  JSON_TYPE_KIND_ARRAY,
};

typedef struct json_type_t   json_type_t;
typedef struct json_object_t json_object_t;
typedef struct json_array_t  json_array_t;

typedef struct json_object_property_t json_object_property_t;
struct json_object_property_t {
  json_object_property_t *next;

  string_t     name;
  json_type_t *value;
};

struct json_object_t {
  int32_t                 num_properties;
  json_object_property_t *properties;
};

typedef struct json_array_value_t json_array_value_t;
struct json_array_value_t {
  json_array_value_t *next;
  json_type_t *value;
};

struct json_array_t {
  int32_t             num_values;
  json_array_value_t *values;
};

struct json_type_t {
  json_type_kind_t kind;
  union {
    json_object_t *object;
    json_array_t  *array;
    double         number; // TODO: int? float?
    string_t       string;
  };
};

typedef struct json_t json_t;
struct json_t {
  string_t source;

  json_type_t *root;

  arena_t *arena;
};

// TODO: TOKENIZER!?
typedef enum token_kind_t token_kind_t;
enum token_kind_t {
  TOKEN_KIND_NONE = 0,

  // ascii tokens... {, [, ", : (others?)

  TOKEN_KIND_NULL = 256,
  TOKEN_KIND_STRING,
  TOKEN_KIND_LONG,
  TOKEN_KIND_DOUBLE,
  TOKEN_KIND_TRUE,
  TOKEN_KIND_FALSE,

  TOKEN_KIND_EOF,
};

typedef struct token_t token_t;
struct token_t {
  token_kind_t kind;

  union {
    string_t string;
    double   number_double;
    long     number_long;
    bool     value;
  };
};

typedef struct tokenizer_t tokenizer_t;
struct tokenizer_t {
  string_t source;
  int32_t at;

  arena_t *arena;
};

static bool ok(tokenizer_t *tokenizer) {
  bool result = tokenizer->at < tokenizer->source.length;
  return result;
}

static uint8_t next(tokenizer_t *tokenizer) {
  uint8_t result = tokenizer->source.data[tokenizer->at];
  tokenizer->at += 1;
  return result;
}

static uint8_t peek(tokenizer_t *tokenizer) {
  uint8_t result = tokenizer->source.data[tokenizer->at];
  return result;
}

static bool is_whitespace(char c) {
  bool result = c == ' ' || c == '\t' || c == '\n' || c == '\r';
  return result;
}

static void eat_whitespace(tokenizer_t *tokenizer) {
  while (ok(tokenizer)) {
    char c = peek(tokenizer);
    if (!is_whitespace(c)) break;

    next(tokenizer);
  }
}

static bool word_matches(tokenizer_t *tokenizer, string_t word) {
  bool result = true;

  int32_t at = tokenizer->at;
  int32_t i = 0;
  while (ok(tokenizer)) {
    uint8_t c = peek(tokenizer);

    if (i >= word.length)  break;
    if (word.data[i] != c) break;

    i += 1;
    next(tokenizer);
  }

  if (i != word.length) {
    result = false;
    tokenizer->at = at;
  }

  return result;
}

static void token_print(token_t *token) {
  switch (token->kind) {
  case TOKEN_KIND_NONE: printf("NONE"); break;
  case TOKEN_KIND_NULL: printf("null"); break;
  case TOKEN_KIND_TRUE: printf("true"); break;
  case TOKEN_KIND_FALSE: printf("false"); break;
  case TOKEN_KIND_STRING: printf("%.*s", token->string.length, token->string.data); break;
  case TOKEN_KIND_LONG: printf("%ld", token->number_long); break;
  case TOKEN_KIND_DOUBLE: printf("%f", token->number_double); break;
  case TOKEN_KIND_EOF: printf("EOF"); break;
  default: printf("%c", (char)token->kind); break;
  }
}

static token_t *next_token(tokenizer_t *tokenizer) {
  token_t *token = NEW(tokenizer, token_t);

  eat_whitespace(tokenizer);

  if (!ok(tokenizer)) {
    token->kind = TOKEN_KIND_EOF;
    return token;
  }

  uint32_t start = tokenizer->at;

  char c = next(tokenizer);

  switch (c) {
    case '[':
    case ']':
    case '{':
    case '}':
    case ':':
    case '"':
    case ',':
      {
        token->kind = c;
        token->string.data = &tokenizer->source.data[start];
        token->string.length = 1;
      } break;
  }

  if (token->kind == TOKEN_KIND_NONE) {
    if (isalpha(c)) {
      token->string.data = &tokenizer->source.data[start];

      // TODO: handle uppercase??

      if (word_matches(tokenizer, STR("true"))) {
        token->kind = TOKEN_KIND_TRUE;
      } else if (word_matches(tokenizer, STR("false"))) {
        token->kind = TOKEN_KIND_FALSE;
      } else if (word_matches(tokenizer, STR("null"))) {
        token->kind = TOKEN_KIND_NULL;
      } else {
        token->kind = TOKEN_KIND_STRING;

        while (ok(tokenizer)) {
          c = peek(tokenizer);
          if (isalpha(c) || isdigit(c) || c == '-' || c == '_') {
            next(tokenizer);
          } else {
            break;
          }
        }
      }

      token->string.length = tokenizer->at - start;
    } else if (isdigit(c) || c == '-' || c == '+') {
      bool isDouble = false;
      while (ok(tokenizer)) {
        c = peek(tokenizer);
        // TODO: verify only 1 dot
        if (isdigit(c) || c == '.') {
          if (c == '.') {
            isDouble = true;
          }
          next(tokenizer);
        } else {
          break;
        }
      }

      uint32_t size = tokenizer->at - start;
      // TODO: "reset" this new
      char *buf = NEW_SIZE(tokenizer, char, size + 1);
      memset(buf, 0, size + 1);
      memcpy(buf, &tokenizer->source.data[start], size);

      if (isDouble) {
        token->kind = TOKEN_KIND_DOUBLE;
        token->number_double = atof(buf);
      } else {
        token->kind = TOKEN_KIND_LONG;
        token->number_long = strtol(buf, 0, 10);
      }
    }
  }

#if TRACE
  token_print(token);
  printf("\n");
#endif

  return token;
}

static json_type_t null_type = { JSON_TYPE_KIND_NULL };
static json_object_property_t null_property = { 0, { "NULL", 4 }, &null_type };

static json_type_t *json_begin_array(json_t *json, tokenizer_t *tokenizer);

static json_type_t *json_begin_object(json_t *json, tokenizer_t *tokenizer) {
  json_type_t *result = NEW(json, json_type_t);
  result->kind = JSON_TYPE_KIND_OBJECT;

  json_object_t *object = NEW(json, json_object_t);
  json_object_property_t *prop = NEW(json, json_object_property_t);

  object->num_properties = 1;
  object->properties = prop;

  result->object = object;

  token_t *token;
  while (true) {
    token = next_token(tokenizer);
    assert(token->kind == '"');

    token = next_token(tokenizer);
    assert(token->kind == TOKEN_KIND_STRING);
    prop->name = token->string;

    token = next_token(tokenizer);
    assert(token->kind == '"');
    token = next_token(tokenizer);
    assert(token->kind == ':');

    token = next_token(tokenizer);
    if (token->kind == '{') {
      prop->value = json_begin_object(json, tokenizer);
    } else if (token->kind == '[') {
      prop->value = json_begin_array(json, tokenizer);
    } else if (token->kind == '"') {
      token = next_token(tokenizer);
      assert(token->kind == TOKEN_KIND_STRING);

      json_type_t *value = NEW(json, json_type_t);
      value->kind = JSON_TYPE_KIND_STRING;
      value->string = token->string;

      prop->value = value;

      token = next_token(tokenizer);
      assert(token->kind == '"');
    } else if (token->kind == TOKEN_KIND_LONG) {
      json_type_t *value = NEW(json, json_type_t);
      value->kind = JSON_TYPE_KIND_NUMBER;
      value->number = token->number_long;

      prop->value = value;
    } else if (token->kind == TOKEN_KIND_DOUBLE) {
      json_type_t *value = NEW(json, json_type_t);
      value->kind = JSON_TYPE_KIND_NUMBER;
      value->number = token->number_double;

      prop->value = value;
    // } else if (token->kind == TOKEN_KIND_BOOL) {
    //    TODO: ...
    // }
    } else {
      fprintf(stderr, "Unexpected token: \"");
      token_print(token);
      printf("\" @ %d\n", tokenizer->at);
      result->kind = JSON_TYPE_KIND_NONE;
      return result;
    }

    if (prop->value->kind == JSON_TYPE_KIND_NONE){
      result->kind = JSON_TYPE_KIND_NONE;
      return result;
    }

    token = next_token(tokenizer);
    if (token->kind == ',') {
      object->num_properties += 1;
      prop->next = NEW(json, json_object_property_t);
      prop = prop->next;
      continue;
    } else if (token->kind == '}') {
      break;
    } else {
      fprintf(stderr, "Unexpected token: \"");
      token_print(token);
      printf("\" @ %d\n", tokenizer->at);
      result->kind = JSON_TYPE_KIND_NONE;
      return result;
    }
  }

  return result;
}

static json_type_t *json_begin_array(json_t *json, tokenizer_t *tokenizer) {
  json_type_t *result = NEW(json, json_type_t);
  result->kind = JSON_TYPE_KIND_ARRAY;

  json_array_t *array = NEW(json, json_array_t);
  json_array_value_t *array_value = NEW(json, json_array_value_t);

  array->values = array_value;
  array->num_values = 1;

  result->array = array;

  token_t *token;
  while (true) {
    token = next_token(tokenizer);
    if (token->kind == '{') {
      array_value->value = json_begin_object(json, tokenizer);
    } else if (token->kind == '[') {
      array_value->value = json_begin_array(json, tokenizer);
    } else if (token->kind == '"') {
      token = next_token(tokenizer);
      assert(token->kind == TOKEN_KIND_STRING);

      json_type_t *value = NEW(json, json_type_t);
      value->kind = JSON_TYPE_KIND_STRING;
      value->string = token->string;

      array_value->value = value;

      token = next_token(tokenizer);
      assert(token->kind == '"');
    } else if (token->kind == TOKEN_KIND_LONG) {
      json_type_t *value = NEW(json, json_type_t);
      value->kind = JSON_TYPE_KIND_NUMBER;
      value->number = token->number_long;

      array_value->value = value;
    } else if (token->kind == TOKEN_KIND_DOUBLE) {
      json_type_t *value = NEW(json, json_type_t);
      value->kind = JSON_TYPE_KIND_NUMBER;
      value->number = token->number_double;

      array_value->value = value;
    // } else if (token->kind == TOKEN_KIND_BOOL) {
    //    TODO: ...
    // }
    } else {
      fprintf(stderr, "Unexpected token: \"");
      token_print(token);
      printf("\" @ %d\n", tokenizer->at);
      result->kind = JSON_TYPE_KIND_NONE;
      return result;
    }

    if (array_value->value->kind == JSON_TYPE_KIND_NONE){
      result->kind = JSON_TYPE_KIND_NONE;
      return result;
    }

    token = next_token(tokenizer);
    if (token->kind == ',') {
      array->num_values += 1;
      array_value->next = NEW(json, json_array_value_t);
      array_value = array_value->next;
      continue;
    } else if (token->kind == ']') {
      break;
    } else {
      fprintf(stderr, "Unexpected token: \"");
      token_print(token);
      printf("\" @ %d\n", tokenizer->at);
      result->kind = JSON_TYPE_KIND_NONE;
      return result;
    }
  }

  return result;
}

static void json_decode(json_t *json) {
  if (json->root->kind != JSON_TYPE_KIND_NULL) {
    // already decoded or just null?
    return;
  }

  tokenizer_t tokenizer = { json->source };
#if !USE_MALLOC
  tokenizer.arena = json->arena;
#endif

  token_t *token = next_token(&tokenizer);
  if (token->kind == '{') {
    json->root = json_begin_object(json, &tokenizer);
  } else if (token->kind == ']') {
    json->root = json_begin_array(json, &tokenizer);
  } else {
    // TODO: error
  }
}

static void json_print_type(json_type_t *it, uint32_t *depth);

static void json_print_array(json_array_t *it, uint32_t *depth) {
  printf("[\n");
  json_array_value_t *value = it->values;
  for (int32_t i = 0; i < it->num_values; i += 1) {
    json_print_type(value->value, depth);
    printf("\n");
    value = value->next;
  }
  printf("]\n");
}

static void json_print_object(json_object_t *it, uint32_t *depth) {
  printf("{\n");
  json_object_property_t *prop = it->properties;
  for (int32_t i = 0; i < it->num_properties; i += 1) {
    printf("%.*s: ", prop->name.length, prop->name.data);
    json_print_type(prop->value, depth);
    printf("\n");
    prop = prop->next;
  }
  printf("}\n");
}

static void json_print_type(json_type_t *it, uint32_t *depth) {
  switch (it->kind) {
  case JSON_TYPE_KIND_NULL: printf("---"); break;
  case JSON_TYPE_KIND_NUMBER: printf("%f", it->number); break;
  case JSON_TYPE_KIND_STRING: printf("%.*s", it->string.length, it->string.data); break;
  case JSON_TYPE_KIND_OBJECT:
    *depth += 1;
    json_print_object(it->object, depth);
    break;
  case JSON_TYPE_KIND_ARRAY:
    *depth += 1;
    json_print_array(it->array, depth);
    break;
  }
}

static void json_print(json_t *json) {
  uint32_t depth = 0;
  json_print_type(json->root, &depth);
}

#define KB(N) (N) * 1024
#define MB(N) (N) * KB(1024)
#define GB(N) (N) * MB(1024)

static json_t decode_json(string_t source) {

  json_t json = { 0 };
  json.root = &null_type;
  json.source = source;

#if !USE_MALLOC
  int64_t capacity = GB(1);
  arena_t *arena = arena_create(capacity);
  json.arena = arena;
#endif

  json_decode(&json);

  return json;
}

static json_type_t *json_find_property(json_type_t *root, string_t property) {
  json_type_t *result = &null_type;

  if (root->kind == JSON_TYPE_KIND_OBJECT) {
    json_object_t *object = root->object;
    json_object_property_t *prop = object->properties;

    while (prop) {
      if (string_equals(prop->name, property)) {
        result = prop->value;
        break;
      }

      prop = prop->next;
    }
  }

  return result;
}

static int decode_data(char *filename, char *answers_filename) {
  uint64_t os_freq = os_timer_frequency();

  uint64_t cpu_start = cpu_timer_read();
  uint64_t os_start = os_timer_read();

  string_t source = read_entire_file(filename);
  if (source.length == 0) {
    return 1;
  }
  uint64_t timer_read = cpu_timer_read();

  double *answers = NULL;
  if (answers_filename) {
    FILE *f = fopen(answers_filename, "rb");
    if (f) {
      struct __stat64 stat;
      _stat64(answers_filename, &stat);

      answers = (double *)malloc(stat.st_size);
      if (answers) {
        if (fread(answers, stat.st_size, 1, f) != 1) {
          fprintf(stderr, "Failed to open answers file: \"%s\"\n", answers_filename);
          free(answers);
          answers = NULL;
        }
      }

      fclose(f);

      if (!answers) {
        return 1;
      }
    }
  }
  uint64_t timer_read_answers = cpu_timer_read();

  json_t json = decode_json(source);
  uint64_t timer_parse = cpu_timer_read();
  if (json.root->kind != JSON_TYPE_KIND_NULL) {
    json_type_t *coords = json_find_property(json.root, STR("coords"));
    if (coords && coords->kind == JSON_TYPE_KIND_ARRAY) {
      double avg = 0;

      json_array_t *arr = coords->array;

      double *answer = answers;

      json_array_value_t *value = arr->values;
      while (value) {
        json_type_t *x0 = json_find_property(value->value, STR("x0"));
        json_type_t *y0 = json_find_property(value->value, STR("y0"));
        json_type_t *x1 = json_find_property(value->value, STR("x1"));
        json_type_t *y1 = json_find_property(value->value, STR("y1"));

        if (
          x0 && x0->kind == JSON_TYPE_KIND_NUMBER &&
          y0 && y0->kind == JSON_TYPE_KIND_NUMBER &&
          x1 && x1->kind == JSON_TYPE_KIND_NUMBER &&
          y1 && y1->kind == JSON_TYPE_KIND_NUMBER
        ) {
          double haversine = ReferenceHaversine(x0->number, y0->number, x1->number, y1->number, EARTH_RADIUS);

          if (answer) {
            if (*answer != haversine) {
              fprintf(stderr, "Expected haversine %f, got %f\n", *answer, haversine);
              return 1;
            }

            answer += 1;
          }

          avg += haversine;

          value = value->next;
        } else {
          fprintf(stderr, "Failed to parse coord pair\n");
          return 1;
        }
      }

      double haversum = avg / (double) arr->num_values;

      if (answer && *answer != haversum) {
        fprintf(stderr, "Expected avg %f, got %f\n", *answer, avg);
        return 1;
      }

      uint64_t timer_sum = cpu_timer_read();

      uint64_t os_end = os_timer_read();
      uint64_t os_elapsed = os_end - os_start;
      uint64_t cpu_end = cpu_timer_read();
      uint64_t cpu_elapsed = cpu_end - cpu_start;
      uint64_t cpu_freq = os_elapsed ? (os_freq * cpu_elapsed / os_elapsed) : 0;

      uint64_t read = timer_read - cpu_start;
      double read_pct = (double)read / (double)cpu_elapsed * 100.0;

      uint64_t read_answers = timer_read_answers - timer_read;
      double read_answers_pct = (double)read_answers / (double)cpu_elapsed * 100.0;

      uint64_t parse = timer_parse - timer_read_answers;
      double parse_pct = (double)parse / (double)cpu_elapsed * 100.0;

      uint64_t sum = timer_sum - timer_parse;
      double sum_pct = (double)sum / (double)cpu_elapsed * 100.0;

      double total_time = (double)os_elapsed / (double)os_freq * 1000;

      printf("\n");
      printf("Input size: %d\n", source.length);
      printf("Pair count: %d\n", arr->num_values);
      printf("Haversine sum: %f\n", haversum);
      printf("\n");
      printf("Total time: %fms (CPU freq %lld)\n", total_time, cpu_freq);
      // printf("  Startup: %lld (%.2f%%)\n", (uint64_t)0, 0.0); ???
      printf("  Read: %lld (%.2f%%)\n", read, read_pct);
      printf("  Read Answers: %lld (%.2f%%)\n", read_answers, read_answers_pct);
      printf("  Parse: %lld (%.2f%%)\n", parse, parse_pct);
      printf("  Sum: %lld (%.2f%%)\n", sum, sum_pct);
      // printf("  Output: %lld (%.2f%%)\n", (uint64_t)0, 0.0); ???
    } else {
      fprintf(stderr, "Failed to find \"coords\" array\n");
      return 1;
    }
  } else {
    // TODO: record error in json
    fprintf(stderr, "Failed to parse json\n");
    return 1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

static void print_usage(char *cmd) {
  printf("Usage: %s generate <seed> <num-pairs>\n", cmd);
  printf("       %s decode <json>\n", cmd);
  printf("\n");
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  char *command = argv[1];

  if (strcmp(command, "generate") == 0 && argc == 4) {
    int32_t seed = atoi(argv[2]);
    int32_t num_coordinates = atoi(argv[3]);

    return generate_data(seed, num_coordinates);
  } else if (strcmp(command, "decode") == 0 && argc > 2) {
    char *filename = argv[2];
    char *answers_filename = argc > 2 ? argv[3] : NULL;

    return decode_data(filename, answers_filename);
  } else {
    print_usage(argv[0]);
    return 1;
  }
}

