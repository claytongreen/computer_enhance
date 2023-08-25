#ifndef OS_H
#define OS_H

#include <inttypes.h>
#include <stdio.h>

static void os_init(void);

static void *os_memory_reserve(uint64_t size);
static void *os_memory_commit(void *data, uint64_t size);
static void *os_memory_zero(void *data, uint64_t size);
static void  os_memory_decommit(void *data, uint64_t size);
static void  os_memory_release(void *data);

static uint64_t os_timer_frequency(void);
static uint64_t os_timer_read(void);

#endif // OS_H
