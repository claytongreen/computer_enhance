// TODO: What do if calling functions multiple times??

static uint64_t cpu_timer_read(void) {
  return __rdtsc();
}

static uint64_t estimate_cpu_timer_freq(void) {
  uint64_t os_freq = os_timer_frequency(); // perf-counts / second

  // perf-counts / second * (second) -> perf-counts
  enum { MS_TO_WAIT = 100 };
  uint64_t os_wait_time = os_freq * MS_TO_WAIT / 1000;

  uint64_t cpu_start = cpu_timer_read(); // clocks

  uint64_t os_start = os_timer_read(); // perf-counts
  uint64_t os_end = 0; // perf-counts
  uint64_t os_elapsed = 0; // perf-counts

  while (os_elapsed < os_wait_time) {
    os_end = os_timer_read();
    os_elapsed = os_end - os_start;
  }

  uint64_t cpu_end = cpu_timer_read(); // clocks
  uint64_t cpu_elapsed = cpu_end - cpu_start; // clocks

  uint64_t cpu_freq = 0; // clocks / second
  if (os_elapsed) {
    //  perf_counts     clocks         1           clocks
    //  -----------  x  ------  x  -----------  =  ------
    //   second           1        perf-counts     second
    cpu_freq = os_freq * cpu_elapsed / os_elapsed;
  }

  return cpu_freq;
}

#ifndef PROFILE
#define PROFILE 1
#endif

#if PROFILE

#define CONCAT2(A, B) A##B
#define CONCAT(A, B) CONCAT2(A, B)

#define TIME_BLOCK_NAME CONCAT(profile_block_, __LINE__)

#define TIME_BLOCK_BEGIN(NAME, BYTES) Profile_Block TIME_BLOCK_NAME = profile_enter((NAME), (__COUNTER__ + 1), BYTES)
#define TIME_BLOCK_END(BLOCK) profile_leave(BLOCK)

#define TIME_BANDWIDTH(NAME, BYTES) \
for(\
TIME_BLOCK_BEGIN(NAME, BYTES); \
TIME_BLOCK_NAME.i < 1; \
TIME_BLOCK_NAME.i += (TIME_BLOCK_END(TIME_BLOCK_NAME), 1)\
)

#define TIME_BLOCK(NAME) TIME_BANDWIDTH(NAME, 0)
#define TIME_FUNC() TIME_BLOCK(__func__)

typedef struct Profile_Point Profile_Point;
struct Profile_Point {
  uint32_t hit_count;

  uint64_t tsc_elapsed_inclusive; // DOES     include children
  uint64_t tsc_elapsed_exclusive; // does NOT include children
  uint64_t processed_byte_count;

  int32_t parent;

  char *name;
};

typedef struct Profile_Block Profile_Block;
struct Profile_Block {
  // used in the TIME_BLOCK macro for hacky defer garbage
  int8_t i;

  int32_t parent_index;
  int32_t point_index;

  uint64_t tsc_start;
  uint64_t old_tsc_elapsed_inclusive;

  char *name;
};

#define MAX_PROFILE_POINTS 128
static Profile_Point points[MAX_PROFILE_POINTS];
static int32_t num_points;
static int32_t active_block;

static Profile_Block profile_enter(char *name, int32_t point_index, uint64_t byte_count) {
  ASSERT(point_index < MAX_PROFILE_POINTS);

  Profile_Point *point = points + point_index;

  point->processed_byte_count += byte_count;

  Profile_Block block = {
    .i = 0,
    .point_index = point_index,
    .parent_index = active_block,
    .name = name,
    .old_tsc_elapsed_inclusive = point->tsc_elapsed_inclusive,
  };

  active_block = block.point_index;
  // TODO: There _has_ to be a better way!
  if ((point_index + 1) > num_points) {
    num_points = point_index + 1;
  }

  block.tsc_start = cpu_timer_read();

  return block;
}

static void profile_leave(Profile_Block block) {
  uint64_t tsc_elapsed = cpu_timer_read() - block.tsc_start;

  active_block = block.parent_index;

  Profile_Point *parent = points + block.parent_index;
  Profile_Point *point  = points + block.point_index;

  parent->tsc_elapsed_exclusive -= tsc_elapsed;
  point->tsc_elapsed_exclusive  += tsc_elapsed;
  point->tsc_elapsed_inclusive   = block.old_tsc_elapsed_inclusive + tsc_elapsed;

  point->hit_count += 1;
  point->name = block.name;
}

static void profile_print_blocks(uint64_t cpu_duration, uint64_t cpu_freq) {
  double pct_denom = 1 / (double)cpu_duration * 100.0;

  for (int32_t i = 0; i < num_points; i += 1) {
    Profile_Point *point = points + i;
    if (point->name) {
      double percent = (double)point->tsc_elapsed_exclusive * pct_denom;

      printf("  %s (%d): %lld (%.2f%%",
        point->name,
        point->hit_count,
        point->tsc_elapsed_exclusive,
        percent
      );

      if (point->tsc_elapsed_inclusive != point->tsc_elapsed_exclusive) {
        double percent_with_children = (double)point->tsc_elapsed_inclusive * pct_denom;
        printf(", %.2f%% w/ children", percent_with_children);
      }
      printf(")");

      if (point->processed_byte_count) {
        double mb = 1024.0 * 1024.0;
        double gb = mb * 1024.0;

        double seconds = (double)point->tsc_elapsed_inclusive / (double)cpu_freq;
        double bytes_per_second = (double)point->processed_byte_count / seconds;
        double mbs = (double)point->processed_byte_count / mb;
        double gbs_per_second = bytes_per_second / gb;

        printf("  %.3fmb @ %.2fgb/s", mbs, gbs_per_second);
      }

      printf("\n");
    }
  }
}


#else

#define TIME_BLOCK(NAME)
#define TIME_FUNC()
#define TIME_BANDWIDTH(...)

#define profile_print_blocks(...)

#endif

typedef struct Profiler Profiler;
struct Profiler {
  uint64_t cpu_start;
  uint64_t cpu_end;
};

static Profiler profiler;


static void profile_start(void) {
  profiler.cpu_start = cpu_timer_read();
}

static void profile_end(void) {
  profiler.cpu_end = cpu_timer_read();
  uint64_t cpu_freq = estimate_cpu_timer_freq();

  uint64_t cpu_duration = profiler.cpu_end - profiler.cpu_start;

  printf("--------------------------------------------------------------------------------\n");
  if (cpu_freq) {
    double total_time = (double)cpu_duration / (double)cpu_freq * 1000.0;
    printf("Total time: %fms (CPU %lld)\n", total_time, cpu_freq);
  }
  profile_print_blocks(cpu_duration, cpu_freq);
  printf("--------------------------------------------------------------------------------\n");
}
