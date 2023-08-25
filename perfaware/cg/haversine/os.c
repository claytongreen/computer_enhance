#include "os.h"

#pragma warning(disable:4042)

// #define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#pragma warning(default:4042)

static SYSTEM_INFO info;

static void os_init(void) {
  GetSystemInfo(&info);
}

static uint64_t os_page_size(void) {
  return info.dwPageSize;
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

