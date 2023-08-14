// TODO: What do if calling functions multiple times??

static uint64_t cpu_timer_read(void) {
  return __rdtsc();
}

static uint64_t estimate_cpu_timer_freq() {
  uint64_t milliseconds_to_wait = 100;
  uint64_t os_freq = os_timer_frequency();

  uint64_t cpu_start = cpu_timer_read();

  uint64_t os_start = os_timer_read();
  uint64_t os_end = 0;
  uint64_t os_elapsed = 0;
  uint64_t os_wait_time = os_freq * milliseconds_to_wait / 1000;

  while (os_elapsed < os_wait_time) {
    os_end = os_timer_read();
    os_elapsed = os_end - os_start;
  }

  uint64_t cpu_end = cpu_timer_read();
  uint64_t cpu_elapsed = cpu_end - cpu_start;

  uint64_t cpu_freq = 0;
  if (os_elapsed) {
    cpu_freq = os_freq * cpu_elapsed / os_elapsed;
  }

  return cpu_freq;
}

#ifndef PROFILE
#define PROFILE 0
#endif

#if PROFILE

#define CONCAT2(A, B) A##B
#define CONCAT(A, B) CONCAT2(A, B)

#define PROFILE_BLOCK_NAME CONCAT(profile_block_, __LINE__)

#define PROFILE_BLOCK_BEGIN(NAME) Profile_Block PROFILE_BLOCK_NAME = profile_enter((NAME), (__COUNTER__ + 1))
#define PROFILE_BLOCK_END(BLOCK) profile_leave(BLOCK)

#define PROFILE_BLOCK(NAME) \
for(\
PROFILE_BLOCK_BEGIN(NAME); \
PROFILE_BLOCK_NAME.i < 1;\
PROFILE_BLOCK_NAME.i += (PROFILE_BLOCK_END(PROFILE_BLOCK_NAME), 1)\
)

#define PROFILE_FUNC() PROFILE_BLOCK(__func__)

typedef struct Profile_Point Profile_Point;
struct Profile_Point {
  uint32_t hit_count;

  uint64_t tsc_elapsed_inclusive;
  uint64_t tsc_elapsed_exclusive;

  int32_t parent;

  char *name;
};

typedef struct Profile_Block Profile_Block;
struct Profile_Block {
  // used in the PROFILE_BLOCK macro for hacky defer garbage
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

static Profile_Block profile_enter(char *name, int32_t point_index) {
  ASSERT(point_index < MAX_PROFILE_POINTS);

  Profile_Point *point = points + point_index;

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

  Profile_Point *parent = points + block.parent_index;
  Profile_Point *point  = points + block.point_index;

  parent->tsc_elapsed_exclusive -= tsc_elapsed;
  point->tsc_elapsed_exclusive += tsc_elapsed;
  point->tsc_elapsed_inclusive = block.old_tsc_elapsed_inclusive + tsc_elapsed;

  point->hit_count += 1;
  point->name = block.name;

  active_block = block.parent_index;
}

static void profile_print_blocks(uint64_t cpu_duration) {
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

      printf(")\n");
    }
  }
}


#else

#define PROFILE_BLOCK(NAME)
#define PROFILE_FUNC()

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
  printf("PROFILE RESULTS:\n");

  if (cpu_freq) {
    double total_time = (double)cpu_duration / (double)cpu_freq * 1000.0;
    printf("Total time: %fms (CPU %lld)\n", total_time, cpu_freq);
  }

  profile_print_blocks(cpu_duration);

  printf("--------------------------------------------------------------------------------\n");
}
