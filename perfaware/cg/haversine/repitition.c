#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <sys/stat.h>

#ifdef _DEBUG
#define ASSERT(Cond) do { if(!(Cond)) __debugbreak(); } while (0)
#else
#define ASSERT(Cond) ((void)(Cond))
#endif

#define ARRAY_COUNT(A) (sizeof(A) / sizeof((A)[0]))

#include "os.h"

#include "os.c"

#include "profile.c"

enum TesterState {
  TesterState_None = 0,
  TesterState_Testing,
  TesterState_Error,
  TesterState_Finished,
};

struct Tester {
  enum TesterState state;
  uint64_t cpu_freq;
  uint64_t bytes_expected;
  uint64_t bytes_consumed;

  uint64_t time;

  uint64_t min;
  uint64_t max;
  uint64_t avg;
  uint64_t test_count;

  uint64_t tsc_wait;
  uint64_t tsc_start;

  uint32_t time_block_count;
};

static void tester_new_wave(struct Tester *tester, char *name, uint64_t target_bytes) {
  if (tester->state == TesterState_None) {
    tester->state = TesterState_Testing;
    tester->bytes_expected = target_bytes;
  }

  if (tester->state != TesterState_Error) {
    tester->min = ULLONG_MAX;
    tester->max = 0;
    tester->avg = 0;

    tester->time = 0;

    tester->tsc_start = cpu_timer_read();
    tester->test_count = 0;

    tester->state = TesterState_Testing;

    printf("\n");
    printf("| %s\n", name);
  }
}

static void tester_error(struct Tester *tester, char *error) {
  tester->state = TesterState_Error;
  fprintf(stderr, error);
}

static void tester_print_clocks_ms_gbps(struct Tester *tester, char *s, uint64_t clocks) {
  double seconds = (double)clocks / (double)tester->cpu_freq;
  double ms = seconds * 1000.0;
  double bps = (double)tester->bytes_consumed / seconds;
  double gbps = bps / (1024.0 * 1024.0 * 1024.0);
  printf("| %s: %lld  %.4f ms  (%.2f GB/s)", s, clocks, ms, gbps);
}

static uint32_t tester_is_testing(struct Tester *tester) {
  uint64_t now = cpu_timer_read();

  if (tester->test_count) {
    if (tester->bytes_consumed == tester->bytes_expected) {
      uint64_t time = tester->time;

      if (time < tester->min) {
        tester->min = time;
        tester->tsc_start = now;
      }
      if (time > tester->max) {
        tester->max = time;
      }
      tester->avg += time;

      uint64_t tsc_since_change = now - tester->tsc_start;
      if (tsc_since_change > tester->tsc_wait) {
        tester->state = TesterState_Finished;
      }

      tester_print_clocks_ms_gbps(tester, "min", tester->min);
      if (tester->tsc_start != now) {
        uint64_t seconds = ((tester->tsc_wait - tsc_since_change) / tester->cpu_freq) + 1;
        printf("   %lld", seconds);
      }

      if (tester->state == TesterState_Finished) {
        printf("\n");
        tester_print_clocks_ms_gbps(tester, "max", tester->max);
        printf("\n");
        tester_print_clocks_ms_gbps(tester, "avg", tester->avg / tester->test_count);
        printf("\n");
      } else {
        printf("                                                                                \r");
      }

      tester->bytes_consumed = 0;
    } else {
      char msg[256];
      snprintf(msg, 256, "Mismatch byte count. Consumed %lld, expected %lld\n", tester->bytes_consumed, tester->bytes_expected);
      tester_error(tester, msg);
    }
  }

  if (tester->state != TesterState_Error) {
    tester->test_count += 1;
  }

  uint32_t result = tester->state == TesterState_Testing;
  return result;
}

static void tester_begin_time(struct Tester *tester) {
  tester->time_block_count += 1;
  uint64_t time = cpu_timer_read();
  tester->time = time;
}

static void tester_end_time(struct Tester *tester) {
  uint64_t time = cpu_timer_read();
  tester->time = time - tester->time;
  tester->time_block_count -= 1;
}

static void tester_consume_bytes(struct Tester *tester, uint64_t bytes) {
  tester->bytes_consumed += bytes;
}

struct TesterParams {
  uint8_t *buffer;
  uint64_t buffer_length;
  char *filename;
};

typedef void (test_func)(struct Tester *, struct TesterParams);

static void test_fread(struct Tester *tester, struct TesterParams params) {
  while (tester_is_testing(tester)) {
    FILE *file = fopen(params.filename, "rb");

    if (file) {
      tester_begin_time(tester);
      size_t bytes_read = fread(params.buffer, 1, params.buffer_length, file);
      tester_end_time(tester);

      if (bytes_read != -1) {
        tester_consume_bytes(tester, bytes_read);
      } else {
        tester_error(tester, "fread: Failed to read file");
      }

      fclose(file);
    } else {
      tester_error(tester, "fread: Failed to open file");
    }
  }
}

struct Test {
  char *name;
  test_func *func;
};
struct Test tests[] = {
  { "fread", test_fread },
};

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("Testing...\n");

  // TODO: get from args
  char *filename = "build\\data_1000000.json";

  struct __stat64 stat;
  _stat64(filename, &stat);

  size_t buffer_length = stat.st_size;
  uint8_t *buffer = (uint8_t *)malloc(buffer_length);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate %lld bytes\n", buffer_length);
    return 1;
  }

  enum { SECONDS_TO_WAIT = 3 };
  uint64_t cpu_freq = estimate_cpu_timer_freq();

  struct TesterParams params = { buffer, buffer_length, filename };

  struct Tester tester = { 0 };
  tester.cpu_freq = cpu_freq;
  tester.tsc_wait = cpu_freq * SECONDS_TO_WAIT;

  for (;;) {
    for (size_t test_index = 0; test_index < ARRAY_COUNT(tests); test_index += 1) {
      struct Test test = tests[test_index];

      tester_new_wave(&tester, test.name, buffer_length);

      test.func(&tester, params);
    }

    if (tester.state == TesterState_Error) {
      break;
    }
  }
}
