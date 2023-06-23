#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

  double a =
      Square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * Square(sin(dLon / 2));
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
  snprintf(haveranswer_filename, 1024, "build/data_%d_haveranswer.f64",
           num_coord_pairs);
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

#define NEW(PARENT, TYPE) \
  (TYPE *)&(PARENT)->mem[(PARENT)->mem_offset]; \
  (PARENT)->mem_offset += sizeof(TYPE);
#define NEW_SIZE(PARENT, TYPE, SIZE) \
  (TYPE *)&(PARENT)->mem[(PARENT)->mem_offset]; \
  (PARENT)->mem_offset += (SIZE);

typedef struct string_t string_t;
struct string_t {
  char *data;
  uint32_t length;
};

static string_t read_entire_file(char *filename) {
  string_t result = { 0 };

  FILE *file = fopen(filename, "r");
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
        fprintf(stderr, "Read %lld bytes, expectecd %ld bytes\n", bytes_read, length);
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

  uint8_t *mem;
  uint32_t mem_offset;
};

// TODO: TOKENIZER!?
typedef enum token_kind_t token_kind_t;
enum token_kind_t {
  TOKEN_KIND_NONE = 0,

  // ascii tokens... {, [, ", : (others?)

  TOKEN_KIND_STRING = 256,
  TOKEN_KIND_LONG,
  TOKEN_KIND_DOUBLE,
  TOKEN_KIND_BOOL, // true, false

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
  uint32_t at;

  uint8_t *mem;
  uint32_t mem_offset;
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

static void eat_whitespace(tokenizer_t *tokenizer) {
  while (ok(tokenizer)) {
    char c = peek(tokenizer);
    if (c == ' ' || c == '\t' || c == '\n') {
      next(tokenizer);
    } else {
      break;
    }
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
      // TODO: handle bool true/false

      token->kind = TOKEN_KIND_STRING;
      token->string.data = &tokenizer->source.data[start];

      while (ok(tokenizer)) {
        c = peek(tokenizer);
        if (isalpha(c) || isdigit(c) || c == '-' || c == '_') {
          next(tokenizer);
        } else {
          break;
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

  return token;
}

static void token_print(token_t *token) {
  switch (token->kind) {
  case TOKEN_KIND_NONE: printf("NONE"); break;
  case TOKEN_KIND_STRING: printf("%.*s", token->string.length, token->string.data); break;
  case TOKEN_KIND_LONG: printf("%ld", token->number_long); break;
  case TOKEN_KIND_DOUBLE: printf("%f", token->number_double); break;
  case TOKEN_KIND_BOOL: printf("TODO BOOL"); break;
  case TOKEN_KIND_EOF: printf("EOF"); break;
  default: printf("%c", (char)token->kind); break;
  }
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
    token = next_token(tokenizer); assert(token->kind == '"');

    token = next_token(tokenizer); assert(token->kind == TOKEN_KIND_STRING);
    prop->name = token->string;

    token = next_token(tokenizer); assert(token->kind == '"');
    token = next_token(tokenizer); assert(token->kind == ':');

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
      fprintf(stderr, "Unexpected token:\n\t");
      token_print(token);
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
      fprintf(stderr, "Unexpected token:\n\t");
      token_print(token);
      break;
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
      fprintf(stderr, "Unexpected token:\n\t");
      token_print(token);
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
      fprintf(stderr, "Unexpected token:\n\t");
      token_print(token);
      break;
    }
  }

  return result;
}

static void json_decode(json_t *json, uint8_t *mem) {
  if (json->root->kind != JSON_TYPE_KIND_NULL) {
    // already decoded or just null?
    return;
  }

  json->mem = mem;

  tokenizer_t tokenizer = { json->source };
  tokenizer.mem = mem;
  tokenizer.mem_offset = 2048 * 1024;

  token_t *token = next_token(&tokenizer);
  if (token->kind == '{') {
    json->root = json_begin_object(json, &tokenizer);
  } else if (token->kind == ']') {
    json->root = json_begin_array(json, &tokenizer);
  } else {
    // TODO: error
  }

#if 0
  token_t *token;
  while (true) {
    token = next_token(&tokenizer);
    //if (token->kind == TOKEN_KIND_DOUBLE) {
      // token_print(token);
//      printf(" ");
//    }
//    if (token->kind == '}') 
        //printf("\n");

    if (token->kind == TOKEN_KIND_EOF) break;
    if (token->kind == TOKEN_KIND_NONE) break;
  }
#endif
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

static int decode_json(char *filename) {
  string_t source = read_entire_file(filename);
  if (source.length == 0) {
    return 1;
  }

  json_t json = { 0 };
  json.root = &null_type;
  json.source = source;

  // TODO: just get double the length of the json file cause why not
  // uint32_t size = source.length * 2;
  uint32_t size = 4096 * 1024; // 4 mb?
  uint8_t *mem = (uint8_t *)malloc(size);
  memset(mem, 0, size);

  json_decode(&json, mem);

  json_print(&json);

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
  } else if (strcmp(command, "decode") == 0 && argc == 3) {
    char *filename = argv[2];

    return decode_json(filename);
  } else {
    print_usage(argv[0]);
    return 1;
  }
}

