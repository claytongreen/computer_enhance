#include "os.h"

#include "mem.h"

string_t read_entire_file(arena_t *arena, char *filename) {
  string_t result = {};

  unsigned int bytes_read;
  unsigned char *file_data = LoadFileData(filename, &bytes_read);
  if (file_data) {
    result.data = PUSH_ARRAY(arena, u8, bytes_read);
    result.length = bytes_read;

    memcpy(result.data, file_data, bytes_read);

    UnloadFileData(file_data);
  }

  return result;
}
