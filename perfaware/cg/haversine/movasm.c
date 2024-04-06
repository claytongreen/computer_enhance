#define _CRT_SECURE_NO_WARNINGS

#nclude <stdio.h>
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

typedef void (test_func)(uint64_t count, uint8_t *data);

extern void mov_4x2(uint64_t count, uint8_t *data);
extern void mov_4x3(uint64_t count, uint8_t *data);
extern void mov_8x2(uint64_t count, uint8_t *data);
extern void mov_8x3(uint64_t count, uint8_t *data);
extern void mov_16x2(uint64_t count, uint8_t *data);
extern void mov_16x3(uint64_t count, uint8_t *data);
extern void mov_32x2(uint64_t count, uint8_t *data);
extern void mov_32x3(uint64_t count, uint8_t *data);
extern void mov_64x2(uint64_t count, uint8_t *data);
extern void mov_64x3(uint64_t count, uint8_t *data);

struct Test {
  char *name;
  test_func *func;

  struct Tester tester;
};

static struct Test tests[] = {
  { "mov_4x2",  mov_4x2 },
  { "mov_4x3",  mov_4x3 },
  { "mov_8x2",  mov_8x2 },
  { "mov_8x3",  mov_8x3 },
  { "mov_16x2", mov_16x2 },
  { "mov_16x3", mov_16x3 },
  { "mov_32x2", mov_32x2 },
  { "mov_32x3", mov_32x3 },
  { "mov_64x2", mov_64x2 },
  { "mov_64x3", mov_64x3 },
};

static struct Tester testers[ARRAY_COUNT(tests)];

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  os_init();
  uint64_t cpu_freq = estimate_cpu_timer_freq();

  uint64_t buffer_length = 1024 * 1024 * 1024ull;
  void *buffer = malloc(buffer_length);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate %lld bytes\n", buffer_length);
    return 1;
  }
  memset(buffer, 0, buffer_length);

  uint64_t *end = (uint64_t *)((uint8_t *)buffer + buffer_length);
  for (uint64_t *it = (uint64_t *)buffer; it < end; it +=1 ) {
    *it = ~0ull;
  }

  printf("Testing...\n");
  for (;;) {
    for (size_t test_idx = 0; test_idx < ARRAY_COUNT(tests); test_idx += 1) {
      struct Test test = tests[test_idx];
      struct Tester *tester = &testers[test_idx];

      printf("\n| %s\n", test.name);
      tester_new_wave(tester, cpu_freq, buffer_length);

      while (tester_is_testing(tester)) {
        tester_begin_time(tester);
        test.func(buffer_length, (uint8_t *)buffer);
        tester_end_time(tester);

        tester_consume_bytes(tester, buffer_length);
      }
    }
  }
}

