#include <psapi.h>

struct OsMetrics {
  uint32_t initialized;

  HANDLE process_handle;
  SYSTEM_INFO info;
};

static struct OsMetrics os_metrics;

static void os_init(void) {
  if (!os_metrics.initialized) {
    os_metrics.initialized = 1;

    os_metrics.process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    GetSystemInfo(&os_metrics.info);
  }
}

static uint64_t os_page_size(void) {
  return os_metrics.info.dwPageSize;
}

static uint64_t os_page_fault_count(void) {
  PROCESS_MEMORY_COUNTERS_EX memory_counters = { 0 };
  memory_counters.cb = sizeof(memory_counters);
  GetProcessMemoryInfo(os_metrics.process_handle, (PROCESS_MEMORY_COUNTERS *)&memory_counters, sizeof(memory_counters));

  uint64_t result = memory_counters.PageFaultCount;
  return result;
}

static void *os_memory_reserve(uint64_t size) {
  void *result = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
  return result;
}

static void *os_memory_commit(void *data, uint64_t size) {
  if (!data) return data;

  void *result = VirtualAlloc(data, size, MEM_COMMIT, PAGE_READWRITE);
  return result;
}

static void *os_memory_zero(void *data, uint64_t size) {
  void *result = ZeroMemory(data, size);
  return result;
}

static void os_memory_decommit(void *data, uint64_t size) {
  // TODO: return result?
  VirtualFree(data, size, MEM_DECOMMIT);
}

static void os_memory_release(void *data) {
  VirtualFree(data, 0, MEM_RELEASE);
}

static uint64_t os_timer_frequency(void) {
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  return frequency.QuadPart;
}

static uint64_t os_timer_read(void) {
  LARGE_INTEGER value;
  QueryPerformanceCounter(&value);
  return value.QuadPart;
}

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

