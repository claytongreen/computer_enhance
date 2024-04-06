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

typedef void (test_func)(uint64_t count, uint64_t *data);

extern void write_x1(uint64_t count, uint64_t *data);
extern void write_x2(uint64_t count, uint64_t *data);
extern void write_x3(uint64_t count, uint64_t *data);
extern void write_x4(uint64_t count, uint64_t *data);
extern void write_x5(uint64_t count, uint64_t *data);
extern void write_x8(uint64_t count, uint64_t *data);

struct Test {
  char *name;
  test_func *func;

  struct Tester tester;
};

static struct Test tests[] = {
  { "write_x1", write_x1 },
  { "write_x2", write_x2 },
  { "write_x3", write_x3 },
  { "write_x4", write_x4 },
  { "write_x5", write_x5 },
  { "write_x8", write_x8 },
};

static struct Tester testers[ARRAY_COUNT(tests)];

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  os_init();
  uint64_t cpu_freq = estimate_cpu_timer_freq();

  uint64_t buffer_length = 4096;
  uint8_t *buffer = (uint8_t *)malloc(buffer_length);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate %lld bytes\n", buffer_length);
    return 1;
  }
  memset(buffer, 0, buffer_length);
  for (size_t idx = 0; idx < buffer_length; idx += 1) {
    buffer[idx] = (uint8_t)idx;
  }

  uint64_t repeat_count = 1024*1024*1024ull;

  printf("Testing...\n");
  for (;;) {
    for (size_t test_idx = 0; test_idx < ARRAY_COUNT(tests); test_idx += 1) {
      struct Test test = tests[test_idx];
      struct Tester *tester = &testers[test_idx];

      printf("\n| %s\n", test.name);
      tester_new_wave(tester, cpu_freq, repeat_count);

      while (tester_is_testing(tester)) {
        tester_begin_time(tester);
        test.func(repeat_count, (uint64_t *)buffer);
        tester_end_time(tester);

        tester_consume_bytes(tester, repeat_count);
      }
    }
  }
}

