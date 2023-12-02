
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
  uint64_t iteration;

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

  tester->iteration = 0;
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
      result += printf(" %3.7f GB/s", gbps);
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

  tester->iteration += 1;

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

        uint32_t new_min = test.value[TestValue_tsc] < results->min.value[TestValue_tsc];
        if (new_min) {
          results->min = test;

          tester->tsc_start = now;
        }

        if (tester->iteration == 1 || new_min) {
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

