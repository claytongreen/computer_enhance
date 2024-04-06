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

static void test(uint8_t *buffer, uint64_t buffer_length) {
  for (size_t i = 0; i < buffer_length; i += 1) {
    buffer[i] = (uint8_t)i;
  }
}

typedef void (test_func)(uint8_t *buffer, uint64_t buffer_length);

extern void mov_all_bytes_asm(uint8_t *buffer, uint64_t buffer_length);
extern void nop_all_bytes_asm(uint8_t *buffer, uint64_t buffer_length);
extern void nop3_all_bytes_asm(uint8_t *buffer, uint64_t buffer_length);
extern void nop9_all_bytes_asm(uint8_t *buffer, uint64_t buffer_length);
extern void nop32_all_bytes_asm(uint8_t *buffer, uint64_t buffer_length);

struct Test {
  char *name;
  test_func *func;

  struct Tester tester;
};
static struct Test tests[] = {
  // { "test", test },
  { "mov_all_bytes_asm", mov_all_bytes_asm },
  { "nop_all_bytes_asm", nop_all_bytes_asm },
  { "nop3_all_bytes_asm", nop3_all_bytes_asm },
  { "nop9_all_bytes_asm", nop9_all_bytes_asm },
  { "nop32_all_bytes_asm", nop9_all_bytes_asm },
};

static struct Tester testers[ARRAY_COUNT(tests)];
int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  os_init();
  uint64_t cpu_freq = estimate_cpu_timer_freq();

  uint64_t buffer_length = 1024 * 1024 * 1024;
  uint8_t *buffer = (uint8_t *)malloc(buffer_length);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate %lld bytes\n", buffer_length);
    return 1;
  }
  memset(buffer, 0, buffer_length);

  printf("Testing...\n");
  for (;;) {
    for (size_t test_index = 0; test_index < ARRAY_COUNT(tests); test_index += 1) {
      struct Test test = tests[test_index];
      struct Tester *tester = &testers[test_index];
      printf("\n| %s\n", test.name);

      tester_new_wave(tester, cpu_freq, buffer_length);
      while (tester_is_testing(tester)) {
        tester_begin_time(tester);
        test.func(buffer, buffer_length);
        tester_end_time(tester);

        tester_consume_bytes(tester, buffer_length);
      }
    }
  }
}

