
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>

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

enum AllocationType {
  AllocationType_None,
  AllocationType_malloc,

  AllocationType_COUNT,
};

static char *allocation_type_string(enum AllocationType allocation_type) {
  char *result;
  switch (allocation_type) {
    case AllocationType_None: result = ""; break;
    case AllocationType_malloc: result = "malloc"; break;
    default: result = "UNKNOWN"; break;
  }
  return result;
}

enum TesterState {
  TesterState_None = 0,
  TesterState_Testing,
  TesterState_Error,
  TesterState_Finished,
};

struct Tester {
  enum TesterState state;

  uint64_t cpu_freq;
  uint64_t tsc_wait;
  uint64_t bytes_expected;

  uint64_t test_count;

  uint64_t bytes_consumed;
  uint64_t tsc_elapsed;
  uint32_t begin_time_count;
  uint32_t end_time_count;

  uint64_t tsc_start;

  uint64_t min;
  uint64_t min_test;
  uint64_t max;
  uint64_t max_test;
  uint64_t tsc_elapsed_total;
};

static void tester_error(struct Tester *tester, char *error) {
  tester->state = TesterState_Error;
  fprintf(stderr, error);
}

static void tester_new_wave(struct Tester *tester, uint64_t cpu_freq, uint64_t target_bytes) {
  if (tester->state == TesterState_None) {
    tester->state = TesterState_Testing;

    tester->cpu_freq = cpu_freq;

    enum { SECONDS_TO_WAIT = 10 };
    tester->tsc_wait = cpu_freq * SECONDS_TO_WAIT;

    tester->bytes_expected = target_bytes;

    tester->min = (uint64_t)-1;
  } else if (tester->state == TesterState_Finished) {
    tester->state = TesterState_Testing;

    if (tester->cpu_freq != cpu_freq) {
      tester_error(tester, "CPU frequency changed");
    }
    if (tester->bytes_expected != target_bytes) {
      tester_error(tester, "Bytes expected changed");
    }

    // tester->test_count = 0;

    // tester->min = ULLONG_MAX;
    // tester->min_test = 0;
    // tester->max = 0;
    // tester->max_test = 0;
  }

  tester->tsc_start = cpu_timer_read();
}

static void print_clocks_ms_gbps(char *s, double clocks, double bytes_consumed, double cpu_freq) {
  printf("| %s: %10.0f", s, clocks);

  if (clocks && cpu_freq) {
    double seconds = clocks / cpu_freq;
    double ms = seconds * 1000.0;
    double bps = bytes_consumed / seconds;
    double gbps = bps / (1024.0 * 1024.0 * 1024.0);
    printf("  %3.7f ms  (%02.7f GB/s)", ms, gbps);
  }
}

static uint32_t tester_is_testing(struct Tester *tester) {
  if (tester->state == TesterState_Testing) {
    uint64_t now = cpu_timer_read();

    if (tester->begin_time_count) {
      if (tester->begin_time_count != tester->end_time_count) {
        tester_error(tester, "Unbalanced tester_begin_time/tester_end_time");
      }
      if (tester->bytes_consumed != tester->bytes_expected) {
        tester_error(tester, "Consumed bytes mismatch");
      }

      if (tester->state == TesterState_Testing) {
        uint64_t time = tester->tsc_elapsed;

        tester->test_count += 1;
        tester->tsc_elapsed_total += time;

        if (time < tester->min) {
          tester->min = time;
          tester->min_test = tester->test_count;
          tester->tsc_start = now;

          print_clocks_ms_gbps("min", (double)tester->min, (double)tester->bytes_consumed, (double)tester->cpu_freq);
          printf("             \r");
        }
        if (time > tester->max) {
          tester->max = time;
          tester->max_test = tester->test_count;
        }

        tester->bytes_consumed = 0;
        tester->tsc_elapsed = 0;
        tester->begin_time_count = 0;
        tester->end_time_count = 0;
      }
    }

    uint64_t tsc_since_change = now - tester->tsc_start;
    if (tsc_since_change > tester->tsc_wait) {
      printf("                                                                                \r");

      print_clocks_ms_gbps("min", (double)tester->min, (double)tester->bytes_expected, (double)tester->cpu_freq);
      printf(" on TEST %lld", tester->min_test);
      printf("\n");

      print_clocks_ms_gbps("max", (double)tester->max, (double)tester->bytes_expected, (double)tester->cpu_freq);
      printf(" on TEST %lld", tester->max_test);
      printf("\n");

      if (tester->test_count != 0) {
        print_clocks_ms_gbps("avg", (double)tester->tsc_elapsed_total / (double)tester->test_count, (double)tester->bytes_expected, (double)tester->cpu_freq);
        printf(" over %lld tests", tester->test_count);
        printf("\n");
      }

      tester->state = TesterState_Finished;
    }
  }

  uint32_t result = tester->state == TesterState_Testing;
  return result;
}

static void tester_begin_time(struct Tester *tester) {
  tester->begin_time_count += 1;
  tester->tsc_elapsed -= cpu_timer_read();
}

static void tester_end_time(struct Tester *tester) {
  tester->tsc_elapsed += cpu_timer_read();
  tester->end_time_count += 1;
}

static void tester_consume_bytes(struct Tester *tester, uint64_t bytes) {
  tester->bytes_consumed += bytes;
}

struct TesterParams {
  enum AllocationType allocation_type;
  uint8_t *buffer;
  uint64_t buffer_length;
  char *filename;
};

static uint8_t *tester_params_allocate_buffer(struct TesterParams params) {
  uint8_t *result = 0;

  switch (params.allocation_type) {
    case AllocationType_None:
      result = params.buffer;
      break;

    case AllocationType_malloc:
      result = (uint8_t *)malloc(params.buffer_length);
      break;

    default:
      fprintf(stderr, "ERROR: unrecognized allocation type");
      break;
  }

  return result;
}

static void tester_params_release_buffer(struct TesterParams params, uint8_t *buffer) {
  switch (params.allocation_type) {
    case AllocationType_None: break;

    case AllocationType_malloc:
      free(buffer);
      break;

    default:
      fprintf(stderr, "ERROR: unrecognized allocation type");
      break;
  }
}

typedef void (test_func)(struct Tester *, struct TesterParams);

static void test_fread(struct Tester *tester, struct TesterParams params) {
  while (tester_is_testing(tester)) {
    FILE *file = fopen(params.filename, "rb");

    uint8_t *buffer = tester_params_allocate_buffer(params);
    if (!buffer) {
      tester_error(tester, "fread: Failed to allocate buffer");
      break;
    }

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

    tester_params_release_buffer(params, buffer);
  }
}

static void test_win32_readfile(struct Tester *tester, struct TesterParams params) {
  while (tester_is_testing(tester)) {
    HANDLE file = CreateFile(params.filename, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    uint8_t *buffer = tester_params_allocate_buffer(params);
    if (!buffer) {
      tester_error(tester, "ReadFile: failed to allocate buffer");
      break;
    }

    if (file != INVALID_HANDLE_VALUE) {
      DWORD bytes_read;

      tester_begin_time(tester);
      BOOL read_result = ReadFile(file, params.buffer, (DWORD)params.buffer_length, &bytes_read, NULL);
      tester_end_time(tester);

      if (read_result == TRUE && bytes_read == params.buffer_length) {
        tester_consume_bytes(tester, bytes_read);
      } else {
        tester_error(tester, "ReadFile: failed to read file");
      }

      CloseHandle(file);
    } else {
      tester_error(tester, "ReadFile: failed to open file");
    }

    tester_params_release_buffer(params, buffer);
  }
}


struct Test {
  char *name;
  test_func *func;

  struct Tester tester;
};
static struct Test tests[] = {
  { "fread", test_fread },
  { "ReadFile", test_win32_readfile },
};
static struct Tester testers[ARRAY_COUNT(tests)][AllocationType_COUNT];

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


  uint64_t cpu_freq = estimate_cpu_timer_freq();

  struct TesterParams params = { AllocationType_None, buffer, buffer_length, filename };

  for (;;) {
    for (size_t test_index = 0; test_index < ARRAY_COUNT(tests); test_index += 1) {
      struct Test test = tests[test_index];

      for (size_t allocation_type = AllocationType_None; allocation_type < AllocationType_COUNT; allocation_type += 1) {
        params.allocation_type = (enum AllocationType)allocation_type;

        struct Tester *tester = &testers[test_index][allocation_type];

        printf("\n| %s %s\n", test.name, allocation_type_string(params.allocation_type));

        tester_new_wave(tester, cpu_freq, buffer_length);
        test.func(tester, params);
      }
    }
  }
}
