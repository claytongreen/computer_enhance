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

enum AllocationType {
  AllocationType_None,
  AllocationType_malloc,
  AllocationType_VirtualAlloc,

  AllocationType_COUNT,
};

static char *allocation_type_string(enum AllocationType allocation_type) {
  char *result;
  switch (allocation_type) {
    case AllocationType_None: result = ""; break;
    case AllocationType_malloc: result = "malloc"; break;
    case AllocationType_VirtualAlloc: result = "VirtualAlloc"; break;
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

static char *tester_state_string(enum TesterState state) {
  char *result;
  switch (state) {
  case TesterState_None: result = "None"; break;
  case TesterState_Testing: result = "Testing"; break;
  case TesterState_Error: result = "Error"; break;
  case TesterState_Finished: result = "Finished"; break;
  default: result = "UNKNOWN"; break;
  }

  return result;
}

enum TestValue {
  TestValue_test_count,
  TestValue_tsc,
  TestValue_byte_count,
  TestValue_mem_page_fault_count,

  TestValue_COUNT,
};

struct TestValues {
  uint64_t value[TestValue_COUNT];
};

struct TesterResults {
  struct TestValues total;
  struct TestValues min;
  struct TestValues max;
};

struct Tester {
  uint64_t cpu_freq;
  uint64_t bytes_expected;
  uint64_t tsc_wait;
  uint64_t tsc_start;

  enum TesterState state;
  uint32_t begin_time_count;
  uint32_t end_time_count;

  struct TestValues test;
  struct TesterResults results;
};

static void tester_error(struct Tester *tester, char *error) {
  tester->state = TesterState_Error;
  fprintf(stderr, error);
}

static void tester_new_wave(struct Tester *tester, uint64_t cpu_freq, uint64_t target_bytes) {
  if (tester->state == TesterState_None) {
    tester->state = TesterState_Testing;

    tester->cpu_freq = cpu_freq;

    enum { SECONDS_TO_WAIT = 3 };
    tester->tsc_wait = cpu_freq * SECONDS_TO_WAIT;

    tester->bytes_expected = target_bytes;

    tester->results.min.value[TestValue_tsc] = (uint64_t)-1;
  } else if (tester->state == TesterState_Finished) {
    tester->state = TesterState_Testing;

    if (tester->cpu_freq != cpu_freq) {
      tester_error(tester, "CPU frequency changed");
    }
    if (tester->bytes_expected != target_bytes) {
      tester_error(tester, "Bytes expected changed");
    }
  }

  tester->tsc_start = cpu_timer_read();
}

static int test_values_print(char *label, struct TestValues values, uint64_t cpu_freq) {
  int result = 0;

  uint64_t test_count = values.value[TestValue_test_count];
  double divisor = test_count ? (double)test_count : 1;

  double tsc = (double)values.value[TestValue_tsc] / divisor;
  double byte_count = (double)values.value[TestValue_byte_count] / divisor;
  double mem_page_fault_count = (double)values.value[TestValue_mem_page_fault_count] / divisor;

  result += printf("| %s: %10.0f", label, tsc);

  if (cpu_freq) {
    double seconds = tsc / (double)cpu_freq;
    double ms = seconds * 1000.0;
    result += printf(" %3.7fms", ms);

    if (byte_count > 0) {
      double gb = 1024.0 * 1024.0 * 1024.0;
      double gbps = byte_count / (gb * seconds);
      result += printf(" %3.7fGB/s", gbps);
    }
  }

  if (mem_page_fault_count > 0) {
    double kbpf = byte_count / (mem_page_fault_count * 1024.0);
    result += printf(" PF: %0.4f (%0.4f kb/fault)", mem_page_fault_count, kbpf);
  }

  return result;
}

static uint32_t tester_is_testing(struct Tester *tester) {
  static int width = 0;

  if (tester->state == TesterState_Testing) {
    struct TestValues test = tester->test;
    uint64_t now = cpu_timer_read();

    if (tester->begin_time_count) {
      if (tester->begin_time_count != tester->end_time_count) {
        tester_error(tester, "Unbalanced tester_begin_time/tester_end_time");
      }
      if (test.value[TestValue_byte_count] != tester->bytes_expected) {
        tester_error(tester, "Consumed bytes mismatch");
      }

      if (tester->state == TesterState_Testing) {
        struct TesterResults *results = &tester->results;

        if (results->total.value[TestValue_test_count] == 0) {
          width = 0;
        }

        results->total.value[TestValue_test_count] += 1;
        results->total.value[TestValue_tsc] += test.value[TestValue_tsc];
        results->total.value[TestValue_byte_count] += test.value[TestValue_byte_count];
        results->total.value[TestValue_mem_page_fault_count] += test.value[TestValue_mem_page_fault_count];

        if (test.value[TestValue_tsc] < results->min.value[TestValue_tsc]) {
          results->min = test;

          tester->tsc_start = now;

          if (width) {
            printf("%*s\r", width, "");
          }

          width = test_values_print("min", results->min, tester->cpu_freq);
        }
        if (test.value[TestValue_tsc] > results->max.value[TestValue_tsc]) {
          results->max = test;
        }

        tester->begin_time_count = 0;
        tester->end_time_count = 0;

        // reset accumulator for the next test wave
        tester->test = (struct TestValues){ 0 };
      }
    }

    uint64_t tsc_since_change = now - tester->tsc_start;
    if (tsc_since_change > tester->tsc_wait) {
      tester->state = TesterState_Finished;

      if (width) {
        printf("%*s\r", width, "");
      }

      test_values_print("min", tester->results.min, tester->cpu_freq);
      printf("\n");
      test_values_print("max", tester->results.max, tester->cpu_freq);
      printf("\n");
      test_values_print("avg", tester->results.total, tester->cpu_freq);
      printf("\n");
    }
  }

  uint32_t result = tester->state == TesterState_Testing;
  return result;
}

static void tester_begin_time(struct Tester *tester) {
  tester->begin_time_count += 1;

  struct TestValues *test = &tester->test;
  test->value[TestValue_mem_page_fault_count] -= os_page_fault_count();
  test->value[TestValue_tsc]                  -= cpu_timer_read();
}

static void tester_end_time(struct Tester *tester) {
  struct TestValues *test = &tester->test;
  test->value[TestValue_tsc]                  += cpu_timer_read();
  test->value[TestValue_mem_page_fault_count] += os_page_fault_count();

  tester->end_time_count += 1;
}

static void tester_consume_bytes(struct Tester *tester, uint64_t bytes) {
  struct TestValues *test = &tester->test;
  test->value[TestValue_byte_count] += bytes;
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

    case AllocationType_VirtualAlloc:
      result = (uint8_t *)VirtualAlloc(0, params.buffer_length, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
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
      buffer = 0;
      break;

    case AllocationType_VirtualAlloc:
      VirtualFree(buffer, 0, MEM_RELEASE);
      buffer = 0;
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

static void test_write_to_all_bytes(struct Tester *tester, struct TesterParams params) {
  while (tester_is_testing(tester)) {
    uint8_t *buffer = tester_params_allocate_buffer(params);
    if (!buffer) {
      tester_error(tester, "WriteToAllBytes: failed to allocate buffer");
      break;
    }

    tester_begin_time(tester);
    for (size_t i = 0; i < params.buffer_length; i += 1) {
      buffer[i] = (uint8_t)i;
    }
    tester_end_time(tester);

    tester_consume_bytes(tester, params.buffer_length);

    tester_params_release_buffer(params, buffer);
  }
}

static void test_write_to_all_bytes_backwards(struct Tester *tester, struct TesterParams params) {
  while (tester_is_testing(tester)) {
    uint8_t *buffer = tester_params_allocate_buffer(params);
    if (!buffer) {
      tester_error(tester, "WriteToAllBytes: failed to allocate buffer");
      break;
    }

    tester_begin_time(tester);
    for (size_t i = 0; i < params.buffer_length; i += 1) {
      buffer[(params.buffer_length - 1) - i] = (uint8_t)i;
    }
    tester_end_time(tester);

    tester_consume_bytes(tester, params.buffer_length);

    tester_params_release_buffer(params, buffer);
  }
}


struct Test {
  char *name;
  test_func *func;

  struct Tester tester;
};
static struct Test tests[] = {
  { "Write All Bytes", test_write_to_all_bytes },
  { "Write All Bytes Backwards", test_write_to_all_bytes_backwards },
  // { "fread", test_fread },
  // { "ReadFile", test_win32_readfile },
};
static struct Tester testers[ARRAY_COUNT(tests)][AllocationType_COUNT];

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("Testing...\n");

  // TODO: get from args
  char *filename = "build\\data_10000000.json";

  struct __stat64 stat;
  _stat64(filename, &stat);

  size_t buffer_length = stat.st_size;
  uint8_t *buffer = (uint8_t *)malloc(buffer_length);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate %lld bytes\n", buffer_length);
    return 1;
  }

  os_init();

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

