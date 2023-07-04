// TODO: What do if calling functions multiple times??
#ifdef DEBUG

static uint64_t cpu_timer_read(void) {
  return __rdtsc();
}

#define PROFILE_NAME(NAME) PROFILE_##NAME

#define PROFILE_START() profile_start()
#define PROFILE_ENTER(NAME) profile_enter(&PROFILE_NAME(NAME))
#define PROFILE_LEAVE(NAME) profile_leave(&PROFILE_NAME(NAME))
#define PROFILE_END() profile_end()

#define PROFILE_DEFINE(NAME)           \
static Profile PROFILE_NAME(NAME) = {  \
  .name = (#NAME),                     \
  .index = __COUNTER__,                \
}

typedef struct Profile Profile;
struct Profile {
  char *name;
  uint32_t index;

  uint32_t count;
  uint64_t total_ticks;
  uint64_t start;

  Profile *parent;
};

PROFILE_DEFINE(read_entire_file);
PROFILE_DEFINE(json_decode);
PROFILE_DEFINE(calc_haversum);
PROFILE_DEFINE(calc_haversum_loop);


// TODO: is there a compile time way or check to init this? or complain if it wasn't called
static uint64_t os_start = 0;
static uint64_t cpu_start = 0;
static void profile_start(void) {
  cpu_start = cpu_timer_read();
  os_start = os_timer_read();
}

static Profile *active_profile;

static void profile_enter(Profile *profile) {
  uint64_t timer = cpu_timer_read();

  profile->start = timer;
  profile->parent = active_profile;
  profile->count += 1;
  
  active_profile = profile;
}

static void profile_leave(Profile *profile) {
  uint64_t timer = cpu_timer_read();

  profile->total_ticks += (timer - profile->start);
  profile->start = 0;

  active_profile = profile->parent;
}

static void profile_end(void) {
  assert(!active_profile && "Failed to call profile_leave");

  uint64_t os_duration = os_timer_read() - os_start;
  uint64_t cpu_duration = cpu_timer_read() - cpu_start;
  uint64_t os_freq = os_timer_frequency();

  uint64_t cpu_freq = os_duration ? (os_freq * cpu_duration / os_duration) : 0;
  double total_time = (double)os_duration / (double)os_freq * 1000.0;

  printf("--------------------------------------------------------------------------------\n");
  printf("PROFILE RESULTS:\n");
  printf("Total time: %fms (CPU %lld)\n", total_time, cpu_freq);
#define PROFILE_PRINT(NAME) printf("  %s (%d): %lld (%.2f%%)\n", PROFILE_NAME(NAME).name, PROFILE_NAME(NAME).count, PROFILE_NAME(NAME).total_ticks, (float)PROFILE_NAME(NAME).total_ticks / (float)cpu_duration * 100.0)
  PROFILE_PRINT(read_entire_file);
  PROFILE_PRINT(json_decode);
  PROFILE_PRINT(calc_haversum);
  PROFILE_PRINT(calc_haversum_loop);
  // TODO: store these in an array so I don't forget to add them here to be printed
#undef PROFILE_PRINT
  printf("--------------------------------------------------------------------------------\n");
}

#else

#define PROFILE_START()
#define PROFILE_ENTER(...)
#define PROFILE_LEAVE(...)
#define PROFILE_END()

static uint64_t cpu_timer_read(void) {
  return 0;
}

#endif