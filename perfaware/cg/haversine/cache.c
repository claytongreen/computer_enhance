#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <sys/stat.h>

#include <intrin.h>

#ifdef _DEBUG
#define ASSERT(Cond) do { if(!(Cond)) __debugbreak(); } while (0)
#else
#define ASSERT(Cond) ((void)(Cond))
#endif

#define ARRAY_COUNT(A) (sizeof(A) / sizeof((A)[0]))

#include "os.h"

#include "os.c"
#include "profile.c"
#include "tester.c"

typedef struct MaskTester MaskTester;
struct MaskTester {
  struct MaskTester *next;

  size_t test_idx;
  uint64_t mask;
  struct Tester tester;
};

static MaskTester *mask_tester_for_idx(MaskTester *root, size_t test_idx, uint64_t mask)
{
  struct MaskTester *result = 0;
  struct MaskTester *last   = root;
  for (struct MaskTester *t = root; t != 0; t = t->next)
  {
    if (t->test_idx == test_idx && t->mask == mask)
    {
      result = t;
      break;
    }
  }
  if (result == 0)
  {
    result = (struct MaskTester *)os_memory_alloc(sizeof(struct MaskTester));
    result->test_idx = test_idx;
    result->mask = mask;

    last->next = result;
  }
  return result;
}

typedef void (test_func)(uint64_t count, uint8_t *data, uint64_t mask);

extern void mov_32x4(uint64_t count, uint8_t *data, uint64_t mask);
extern void mov_32x6(uint64_t count, uint8_t *data, uint64_t mask);

struct Test {
  char *name;
  test_func *func;
  uint8_t length;
};

static uint64_t next_pow_2(uint64_t n) {
  uint64_t result = n - 1;
  for (uint64_t mask = 1; mask <= 32; mask = mask << 1) { result |= result >> mask; }
  result += 1;
  return result;
}

static struct Test tests[] = {
  { "mov_32x4", mov_32x4, 32*4 },
  { "mov_32x6", mov_32x6, 32*6 },
};

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  os_init();
  uint64_t cpu_freq = estimate_cpu_timer_freq();

  uint64_t buffer_length = 1024 * 1024 * 1024ull; // 1gb
  void *buffer = os_memory_alloc(buffer_length);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate %lld bytes\n", buffer_length);
    return 1;
  }

  // TODO: delete that there file, if it exists
  LPCSTR filename = "cache.csv";
  HANDLE file = CreateFileA(filename, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "Failed to create file %s\n", filename);
    return 2;
  }

  char write_buffer[1024];

  int len = snprintf(write_buffer, ARRAY_COUNT(write_buffer), "test,mask,gbps\n");
  WriteFile(file, write_buffer, len, 0, 0);
  CloseHandle(file);

  struct MaskTester *root = (struct MaskTester *)os_memory_alloc(sizeof(MaskTester));
  root->test_idx = 0;
  root->mask = 0;

  printf("Testing...\n");
  for (;;) {
    for (size_t test_idx = 0; test_idx < ARRAY_COUNT(tests); test_idx += 1) {
      struct Test test = tests[test_idx];

      uint64_t mask = next_pow_2(test.length);
      while (mask < buffer_length) {
        struct MaskTester *t = mask_tester_for_idx(root, test_idx, mask);
        struct Tester *tester = &t->tester;

        printf("\n| %s %lld\n", test.name, mask);
        tester_new_wave(tester, cpu_freq, buffer_length);

        while (tester_is_testing(tester)) {
          tester_begin_time(tester);
          test.func(buffer_length, (uint8_t *)buffer, mask-1);
          tester_end_time(tester);

          tester_consume_bytes(tester, buffer_length);
        }

        file = CreateFileA(filename, FILE_APPEND_DATA | FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (file == INVALID_HANDLE_VALUE) {
          fprintf(stderr, "Failed to open file %s\n", filename);
          return 3;
        }
        double gbps = 0.0;
        if (cpu_freq) {
          uint64_t *values = tester->results.min.value;
          uint64_t tsc = values[TestValue_tsc];
          uint64_t bytes = values[TestValue_byte_count];
          double seconds = (double)tsc / (double)cpu_freq;
          double gb = 1024.0 * 1024.0 * 1024.0;
          gbps = (double)bytes / (gb * seconds);
        }
        SetFilePointer(file, 0, 0, FILE_END);
        len = snprintf(write_buffer, ARRAY_COUNT(write_buffer), "%s,%lld,%f\n", test.name, mask, gbps);
        WriteFile(file, write_buffer, len, 0, 0);
        CloseHandle(file);

        mask <<= 1;
      }
    }
  }
}

