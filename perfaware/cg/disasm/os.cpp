#include "os.h"

#include "mem.h"

string_t read_entire_file(arena_t *arena, char *filename) {
  string_t result = {};

#ifdef RAYLIB_H
  unsigned int bytes_read;
  unsigned char *file_data = LoadFileData(filename, &bytes_read);
  if (file_data) {
    result.data = PUSH_ARRAY(arena, u8, bytes_read);
    result.length = bytes_read;

    memcpy(result.data, file_data, bytes_read);

    UnloadFileData(file_data);
  }
#else
  HANDLE handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle != INVALID_HANDLE_VALUE) {
    DWORD file_size = GetFileSize(handle, NULL);
    result.data = PUSH_ARRAY(arena, u8, file_size);
    result.length = file_size;

    DWORD bytes_read;
    BOOL read_result = ReadFile(handle, result.data, file_size, &bytes_read, NULL);
    if (read_result != TRUE || bytes_read != file_size) {
      result.data = 0;
      result.length = 0;
      // TODO: Reset arena?
    }

    CloseHandle(handle);
  }
#endif

  return result;
}
