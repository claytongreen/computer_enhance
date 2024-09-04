
enum TesterState {
  TesterState_None = 0,
  TesterState_Testing,
  TesterState_Error,
  TesterState_Finished,
};

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
    tester->tsc_wait = cpu_freq * 3; // 3 second timeout
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
  tester->test.value[TestValue_test_count] = 1;
}

static void test_values_print(char *label, struct TestValues values, uint64_t cpu_freq) {
  uint64_t test_count = values.value[TestValue_test_count];
  double divisor = (test_count != 0) ? (double)test_count : 1;

  double tsc = (double)values.value[TestValue_tsc] / divisor;
  double byte_count = (double)values.value[TestValue_byte_count] / divisor;
  double mem_page_fault_count = (double)values.value[TestValue_mem_page_fault_count] / divisor;

  printf("| %s: %10.0f", label, tsc);

  if (cpu_freq) {
    double seconds = tsc / (double)cpu_freq;
    double ms = seconds * 1000.0;
    printf(" %3.7fms", ms);

    if (byte_count > 0) {
      double gb = 1024.0 * 1024.0 * 1024.0;
      double gbps = byte_count / (gb * seconds);
      printf(" %3.7f GB/s", gbps);
    }
  }

  if (mem_page_fault_count > 0) {
    double kbpf = byte_count / (mem_page_fault_count * 1024.0);
    printf(" PF: %0.4f (%0.4f kb/fault)", mem_page_fault_count, kbpf);
  }
}

static uint32_t tester_is_testing(struct Tester *tester) {
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

        for (uint32_t idx = 0; idx < TestValue_COUNT; idx += 1) {
          results->total.value[idx] += test.value[idx];
        }

        uint32_t new_min = (test.value[TestValue_tsc] < results->min.value[TestValue_tsc]) ? 1 : 0;
        uint32_t new_max = (test.value[TestValue_tsc] > results->min.value[TestValue_tsc]) ? 1 : 0;

        if (new_min == 1) {
          results->min = test; // record new min test
          tester->tsc_start = now; // reset timeout

          printf("\033[2K\033[0G"); // clear and reset cursor on current line

          test_values_print("min", results->min, tester->cpu_freq);
        }
        if (new_max == 1) {
          results->max = test; // record new max test
        }

        tester->begin_time_count = 0;
        tester->end_time_count = 0;

        // reset accumulator for the next test wave
        tester->test = (struct TestValues){ 0 };
        tester->test.value[TestValue_test_count] = 1;
      }
    }

    //- cg: timeout, no changes have occurred in `tsc_wait`
    uint64_t tsc_since_change = now - tester->tsc_start;
    if (tsc_since_change > tester->tsc_wait) {
      tester->state = TesterState_Finished;

      printf("\033[2K\033[0G");

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

